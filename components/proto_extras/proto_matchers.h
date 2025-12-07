// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROTO_EXTRAS_PROTO_MATCHERS_H_
#define COMPONENTS_PROTO_EXTRAS_PROTO_MATCHERS_H_

#include <type_traits>

#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace google::protobuf {
template <typename T>
class RepeatedField;
template <typename Key, typename T>
class Map;
}  // namespace google::protobuf

namespace proto_extras {
// Used to force the compiler to resolve the correct overload of a non-ptr
// repeated field accessor.
template <typename MessageType, typename FieldType>
constexpr auto ResolveRepeatedField(
    const ::google::protobuf::RepeatedField<FieldType>& (
        MessageType::*func_ptr)() const) {
  return func_ptr;
}

// Used to force the compiler to resolve the correct overload of a non-ptr
// repeated field accessor.
template <typename MessageType, typename FieldType>
constexpr auto ResolveRepeatedPtrField(
    const ::google::protobuf::RepeatedPtrField<FieldType>& (
        MessageType::*func_ptr)() const) {
  return func_ptr;
}

MATCHER_P5(HasOptionalField,
           property_name,
           has_field_function,
           field_function,
           expected_has_field,
           expected_field_value_matcher,
           "") {
  bool arg_has_field = (arg.*has_field_function)();
  if (arg_has_field != expected_has_field) {
    *result_listener << "is an object whose property `has_" << property_name
                     << "` is `" << arg_has_field << "` but expected `"
                     << expected_has_field << "` ";
    return false;
  }
  if (!arg_has_field) {
    return true;
  }
  return testing::ExplainMatchResult(
      testing::Property(property_name, field_function,
                        expected_field_value_matcher),
      arg, result_listener);
}

// Note: This could be improved to take a vector of matchers, and there would
// need to be a helper method to transform the repeated field list and matcher
// into a vector of applicable matchers.
MATCHER_P3(HasRepeatedField,
           property_name,
           field_function,
           expected_proto_message,
           "") {
  const auto& expected_field_list = (expected_proto_message.*field_function)();
  bool result = testing::ExplainMatchResult(
      testing::Property(property_name, field_function,
                        testing::ElementsAreArray(expected_field_list)),
      arg, result_listener);
  return result;
}

// Note: This could be improved to take a vector of matchers, and there would
// need to be a helper method to transform the repeated field list and matcher
// into a vector of applicable matchers.
MATCHER_P4(HasRepeatedField,
           property_name,
           field_function,
           expected_proto_message,
           item_matcher_function,
           "") {
  // Create a matcher per item in the expected list.
  const auto& expected_field_list = (expected_proto_message.*field_function)();
  using ItemType =
      typename std::decay_t<decltype(expected_field_list)>::value_type;
  std::vector<testing::Matcher<const ItemType&>> matchers;
  matchers.reserve(expected_field_list.size());
  for (const auto& item : expected_field_list) {
    matchers.push_back((*item_matcher_function)(item));
  }

  bool result = testing::ExplainMatchResult(
      testing::Property(property_name, field_function,
                        testing::ElementsAreArray(matchers)),
      arg, result_listener);
  return result;
}

MATCHER_P3(HasMapField,
           property_name,
           field_function,
           expected_proto_message,
           "") {
  const auto& expected_field_map = (expected_proto_message.*field_function)();
  return testing::ExplainMatchResult(
      testing::Property(property_name, field_function,
                        testing::UnorderedElementsAreArray(expected_field_map)),
      arg, result_listener);
}

// Note: This could be improved to take a vector of matchers, and there would
// need to be a helper method to transform a field and matcher into a vector of
// applicable matchers.
MATCHER_P4(HasMapField,
           property_name,
           field_function,
           expected_proto_message,
           value_matcher_function,
           "") {
  const auto& expected_field_map = (expected_proto_message.*field_function)();

  // Correctly deduce KeyType and ValueType from the map's value_type.
  using MapValueType =
      typename std::decay_t<decltype(expected_field_map)>::value_type;
  using KeyType = typename MapValueType::first_type;
  using ValueType = typename MapValueType::second_type;

  // Create a vector of matchers for the expected map entries.
  std::vector<testing::Matcher<std::pair<const KeyType, ValueType>>> matchers;
  matchers.reserve(expected_field_map.size());
  for (const auto& item : expected_field_map) {
    matchers.push_back(testing::Pair(testing::Eq(item.first),
                                     (*value_matcher_function)(item.second)));
  }

  return testing::ExplainMatchResult(
      testing::Property(property_name, field_function,
                        testing::UnorderedElementsAreArray(matchers)),
      arg, result_listener);
}

}  // namespace proto_extras

#endif  // COMPONENTS_PROTO_EXTRAS_PROTO_MATCHERS_H_
