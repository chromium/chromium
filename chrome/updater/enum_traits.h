// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_ENUM_TRAITS_H_
#define CHROME_UPDATER_ENUM_TRAITS_H_

#include <ostream>
#include <type_traits>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

// Provides a way to safely convert numeric types to enumerated values.
// To use this facility, the enum definition must be annotated with traits to
// specify the range of the enum values. Due to how specialization of class
// templates works in C++14, the |EnumTraits| specialization of the primary
// template must be defined inside the |updater| namespace, where the
// primary template is defined. Traits for enum types defined inside
// other scopes, such as nested classes or other namespaces, may not work.
//
// enum class MyEnum {
//   kVal1 = -1,
//   kVal2 = 0,
//   kVal3 = 1,
// };
//
// template <>
// struct EnumTraits<MyEnum> {
//   static constexpr MyEnum first_elem = MyEnum::kVal1;
//   static constexpr MyEnum last_elem = MyEnum::kVal3;
// };
//
// MyEnum val = *CheckedCastToEnum<MyEnum>(-1);

template <typename T>
struct EnumTraits {};

// Returns an optional value of an enum type T if the conversion from an
// integral type V is safe, meaning that |v| is within the bounds of the enum.
// The enum type must be annotated with traits to specify the lower and upper
// bounds of the enum values.
template <typename T, typename V>
absl::optional<T> CheckedCastToEnum(V v) {
  static_assert(std::is_enum<T>::value, "T must be an enum type.");
  static_assert(std::is_integral<V>::value, "V must be an integral type.");
  using Traits = EnumTraits<T>;
  return (static_cast<V>(Traits::first_elem) <= v &&
          v <= static_cast<V>(Traits::last_elem))
             ? absl::make_optional(static_cast<T>(v))
             : absl::nullopt;
}

}  // namespace updater

#endif  // CHROME_UPDATER_ENUM_TRAITS_H_
