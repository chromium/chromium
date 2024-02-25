// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_TYPE_HELPERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_TYPE_HELPERS_H_

#include <optional>
#include <type_traits>

#include "base/notreached.h"
#include "base/types/optional_ref.h"
#include "base/types/optional_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace resource_attribution {

namespace internal {

// The constant IsVariantAlternative<T, V>::value is true iff T is one of the
// alternative types of variant V.
template <typename T, typename V, size_t I = absl::variant_size<V>::value>
struct IsVariantAlternative
    : std::disjunction<
          std::is_same<T, typename absl::variant_alternative<I - 1, V>::type>,
          IsVariantAlternative<T, V, I - 1>> {};

template <typename T, typename V>
struct IsVariantAlternative<T, V, 0> : std::false_type {};

// For SFINAE, a template using EnableIfIsVariantAlternative<T, V> will only
// match if T is one of the alternative types of variant V.
template <typename T, typename V>
using EnableIfIsVariantAlternative =
    std::enable_if_t<IsVariantAlternative<T, V>::value, bool>;

// If `v`, a variant of type V, currently holds an alternative of type T,
// returns that alternative. Otherwise returns nullopt.
template <typename T, typename V, EnableIfIsVariantAlternative<T, V> = true>
constexpr std::optional<T> GetAsOptional(const V& v) {
  return base::OptionalFromPtr(absl::get_if<T>(&v));
}

// Returns true iff any element of `vs`, a vector of variants of type V,
// currently holds an alternative of type T.

// Look up `T` in `variant<T, ...>`.
template <typename T, typename V, EnableIfIsVariantAlternative<T, V> = true>
constexpr bool VariantVectorContains(const std::vector<V>& vs) {
  for (const V& v : vs) {
    if (absl::holds_alternative<T>(v)) {
      return true;
    }
  }
  return false;
}

// Look up `const T` in `variant<T, ...>`.
template <typename ConstT,
          typename V,
          std::enable_if_t<std::is_const_v<ConstT>, bool> = true,
          EnableIfIsVariantAlternative<std::remove_const_t<ConstT>, V> = true>
constexpr bool VariantVectorContains(const std::vector<V>& vs) {
  for (const V& v : vs) {
    if (absl::holds_alternative<std::remove_const_t<ConstT>>(v)) {
      return true;
    }
  }
  return false;
}

// If at least one element of `vs`, a vector of variants of type V, currently
// holds an alternative of type T, returns a reference to the first such
// element. Otherwise returns nullopt.

// Look up `T` in `variant<T, ...>`, return `optional_ref<T>`.
template <typename T, typename V, EnableIfIsVariantAlternative<T, V> = true>
constexpr base::optional_ref<T> GetFromVariantVector(std::vector<V>& vs) {
  for (V& v : vs) {
    T* t = absl::get_if<T>(&v);
    if (t) {
      return t;
    }
  }
  return std::nullopt;
}

// Look up `T` in `variant<T, ...>`, return `optional_ref<const T>`.
template <typename T, typename V, EnableIfIsVariantAlternative<T, V> = true>
constexpr base::optional_ref<const T> GetFromVariantVector(
    const std::vector<V>& vs) {
  for (const V& v : vs) {
    const T* t = absl::get_if<T>(&v);
    if (t) {
      return t;
    }
  }
  return std::nullopt;
}

// Look up `const T` in `variant<T, ...>`, return `optional_ref<const T>`.
template <typename ConstT,
          typename V,
          std::enable_if_t<std::is_const_v<ConstT>, bool> = true,
          EnableIfIsVariantAlternative<std::remove_const_t<ConstT>, V> = true>
constexpr base::optional_ref<ConstT> GetFromVariantVector(
    const std::vector<V>& vs) {
  for (const V& v : vs) {
    ConstT* t = absl::get_if<std::remove_const_t<ConstT>>(&v);
    if (t) {
      return t;
    }
  }
  return std::nullopt;
}

// Extended comparators for variants, allowing a variant to be compared with any
// alternative held in it.

template <typename T, typename V, EnableIfIsVariantAlternative<T, V> = true>
constexpr bool operator==(const T& a, const V& b) {
  return absl::holds_alternative<T>(b) && a == absl::get<T>(b);
}

template <typename T, typename V, EnableIfIsVariantAlternative<T, V> = true>
constexpr bool operator==(const V& a, const T& b) {
  return absl::holds_alternative<T>(a) && absl::get<T>(a) == b;
}

template <typename T, typename V, EnableIfIsVariantAlternative<T, V> = true>
constexpr bool operator!=(const T& a, const V& b) {
  return !absl::holds_alternative<T>(b) || a != absl::get<T>(b);
}

template <typename T, typename V, EnableIfIsVariantAlternative<T, V> = true>
constexpr bool operator!=(const V& a, const T& b) {
  return !absl::holds_alternative<T>(a) || absl::get<T>(a) != b;
}

}  // namespace internal

// Enable extended comparators for variants defined in the resource_attribution
// namespace.
using internal::operator==;
using internal::operator!=;

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_TYPE_HELPERS_H_
