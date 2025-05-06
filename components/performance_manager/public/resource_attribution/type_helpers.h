// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_TYPE_HELPERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_TYPE_HELPERS_H_

#include <algorithm>
#include <optional>
#include <type_traits>
#include <variant>

#include "base/notreached.h"
#include "base/types/optional_ref.h"
#include "base/types/optional_util.h"

namespace resource_attribution {

namespace internal {

// The constant IsVariantAlternative<T, V>::value is true iff T is one of the
// alternative types of variant V.
template <typename T, typename V, size_t I = std::variant_size<V>::value>
inline constexpr bool kIsVariantAlternative =
    std::is_same_v<T, typename std::variant_alternative<I - 1, V>::type> ||
    kIsVariantAlternative<T, V, I - 1>;

template <typename T, typename V>
inline constexpr bool kIsVariantAlternative<T, V, 0> = false;

// If `v`, a variant of type V, currently holds an alternative of type T,
// returns that alternative. Otherwise returns nullopt.
template <typename T, typename V>
  requires(kIsVariantAlternative<T, V>)
constexpr std::optional<T> GetAsOptional(const V& v) {
  return base::OptionalFromPtr(std::get_if<T>(&v));
}

// Returns true iff any element of `vs`, a vector of variants of type V,
// currently holds an alternative of type T.

// Look up `T` in `variant<T, ...>`.
template <typename T, typename V>
  requires(kIsVariantAlternative<T, V>)
constexpr bool VariantVectorContains(const std::vector<V>& vs) {
  return std::ranges::any_of(
      vs, [](const V& v) { return std::holds_alternative<T>(v); });
}

// Look up `const T` in `variant<T, ...>`.
template <typename ConstT, typename V>
  requires(std::is_const_v<ConstT> &&
           kIsVariantAlternative<std::remove_const_t<ConstT>, V>)
constexpr bool VariantVectorContains(const std::vector<V>& vs) {
  return std::ranges::any_of(vs, [](const V& v) {
    return std::holds_alternative<std::remove_const_t<ConstT>>(v);
  });
}

// If at least one element of `vs`, a vector of variants of type V, currently
// holds an alternative of type T, returns a reference to the first such
// element. Otherwise returns nullopt.

// Look up `T` in `variant<T, ...>`, return `optional_ref<T>`.
template <typename T, typename V>
  requires(kIsVariantAlternative<T, V>)
constexpr base::optional_ref<T> GetFromVariantVector(std::vector<V>& vs) {
  for (V& v : vs) {
    T* t = std::get_if<T>(&v);
    if (t) {
      return t;
    }
  }
  return std::nullopt;
}

// Look up `T` in `variant<T, ...>`, return `optional_ref<const T>`.
template <typename T, typename V>
  requires(kIsVariantAlternative<T, V>)
constexpr base::optional_ref<const T> GetFromVariantVector(
    const std::vector<V>& vs) {
  for (const V& v : vs) {
    const T* t = std::get_if<T>(&v);
    if (t) {
      return t;
    }
  }
  return std::nullopt;
}

// Look up `const T` in `variant<T, ...>`, return `optional_ref<const T>`.
template <typename ConstT, typename V>
  requires(std::is_const_v<ConstT> &&
           kIsVariantAlternative<std::remove_const_t<ConstT>, V>)
constexpr base::optional_ref<ConstT> GetFromVariantVector(
    const std::vector<V>& vs) {
  for (const V& v : vs) {
    ConstT* t = std::get_if<std::remove_const_t<ConstT>>(&v);
    if (t) {
      return t;
    }
  }
  return std::nullopt;
}

// Extended comparators for variants, allowing a variant to be compared with any
// alternative held in it.

template <typename T, typename V>
  requires(kIsVariantAlternative<T, V>)
constexpr bool operator==(const T& a, const V& b) {
  return std::holds_alternative<T>(b) && a == std::get<T>(b);
}

template <typename T, typename V>
  requires(kIsVariantAlternative<T, V>)
constexpr bool operator==(const V& a, const T& b) {
  return std::holds_alternative<T>(a) && std::get<T>(a) == b;
}

}  // namespace internal

// Enable extended comparators for variants defined in the resource_attribution
// namespace.
using internal::operator==;

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_TYPE_HELPERS_H_
