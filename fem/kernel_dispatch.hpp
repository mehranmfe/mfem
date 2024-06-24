// Copyright (c) 2010-2024, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#ifndef MFEM_KERNELDISPATCH_HPP
#define MFEM_KERNELDISPATCH_HPP

#include "../general/error.hpp"

#include <functional>
#include <unordered_map>
#include <tuple>
#include <cmath>

namespace mfem
{

namespace internal
{
template<typename... Types>
struct KernelTypeList { };
}

// Expands a variable length macro parameter so that multiple variable length
// parameters can be passed to the same macro.
#define MFEM_PARAM_LIST(...) __VA_ARGS__

// Declare the class used to dispatch shared memory kernels when the fallback
// methods don't require template parameters.
#define MFEM_DECLARE_KERNELS(KernelName, KernelType, UserParams)               \
   class KernelName ## Kernels : public                                        \
   KernelDispatchTable<KernelName ## Kernels, KernelType,                      \
      internal::KernelTypeList<UserParams>, internal::KernelTypeList<>>        \
   {                                                                           \
   public:                                                                     \
      using KernelSignature = KernelType;                                      \
      template <int DIM, UserParams>                                           \
      static KernelSignature Kernel();                                         \
      static KernelSignature Fallback(int dim);                                \
      static KernelName ## Kernels &Get()                                      \
      { static KernelName ## Kernels table; return table;}                     \
   };

#define MFEM_DECLARE_KERNELS_2(KernelName, KernelType, UserParams, FallbackParams) \
   class KernelName ## Kernels : public                                        \
   KernelDispatchTable<KernelName ## Kernels, KernelType,                      \
      internal::KernelTypeList<UserParams>,                                    \
      internal::KernelTypeList<FallbackParams>>                                \
   {                                                                           \
   public:                                                                     \
      using KernelSignature = KernelType;                                      \
      template <int DIM, FallbackParams, UserParams>                           \
      static KernelSignature Kernel();                                         \
      template <FallbackParams>                                                \
      static KernelSignature Fallback(int dim);                                \
      static KernelName ## Kernels &Get()                                      \
      { static KernelName ## Kernels table; return table;}                     \
   };

// KernelDispatchKeyHash is a functor that hashes variadic packs for which each
// type contained in the variadic pack has a specialization of `std::hash`
// available.  For example, packs containing int, bool, enum values, etc.
template<typename ...KernelParameters>
struct KernelDispatchKeyHash
{
   using Tuple = std::tuple<KernelParameters...>;

private:
   template<int N>
   size_t operator()(Tuple value) const { return 0; }

   // The hashing formula here is taken directly from the Boost library, with
   // the magic number 0x9e3779b9 chosen to minimize hashing collisions.
   template<std::size_t N, typename THead, typename... TTail>
   size_t operator()(Tuple value) const
   {
      constexpr int Index = N - sizeof...(TTail) - 1;
      auto lhs_hash = std::hash<THead>()(std::get<Index>(value));
      auto rhs_hash = operator()<N, TTail...>(value);
      return lhs_hash ^(rhs_hash + 0x9e3779b9 + (lhs_hash << 6) + (lhs_hash >> 2));
   }

public:
   size_t operator()(Tuple value) const
   {
      return operator()<sizeof...(KernelParameters), KernelParameters...>(value);
   }
};

template<typename... T>
class KernelDispatchTable { };

// KernelDispatchTable is derived from `DispatchTable` using the `KernelDispatchKeyHash` functor above
// to assign specialized kernels with individual keys.
template <typename Kernels, typename Signature, typename... UserParams, typename... FallbackParams>
class KernelDispatchTable<Kernels, Signature, internal::KernelTypeList<UserParams...>, internal::KernelTypeList<FallbackParams...>>
{
   std::unordered_map<std::tuple<int, FallbackParams..., UserParams...>,
       Signature,
       KernelDispatchKeyHash<int, FallbackParams..., UserParams...>> table;

public:
   // TODO(bowen) Force this to use the same signature as the Signature typedef
   // above.
   template<typename... KernelArgs>
   void Run(int dim, UserParams... params, KernelArgs&... args)
   {
      std::tuple<int, UserParams...> key;
      key = std::make_tuple(dim, params...);
      const auto it = this->table.find(key);
      if (it != this->table.end())
      {
         printf("Using specialized kernel\n");
         it->second(args...);
      }
      else
      {
         printf("Using non-specialized kernel\n");
         Kernels::Fallback(dim)(args...);
      }
   }

   template <int DIM, FallbackParams... F_PARAMS, UserParams... U_PARAMS>
   void AddSpecialization()
   {
      std::tuple<int, FallbackParams..., UserParams...>
      param_tuple(DIM, F_PARAMS..., U_PARAMS...);
      table[param_tuple] = Kernels:: template Kernel<DIM, F_PARAMS..., U_PARAMS...>();
   };
};

}

#endif
