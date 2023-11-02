// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_TYPE_HELPERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_TYPE_HELPERS_H_

#include <type_traits>

#include "base/types/optional_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager::resource_attribution {

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
constexpr absl::optional<T> GetAsOptional(const V& v) {
  return base::OptionalFromPtr(absl::get_if<T>(&v));
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

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_TYPE_HELPERS_H_
