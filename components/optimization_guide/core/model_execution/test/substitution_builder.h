// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_SUBSTITUTION_BUILDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_SUBSTITUTION_BUILDER_H_

#include <initializer_list>
#include <optional>
#include <string>
#include <type_traits>

#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"

namespace optimization_guide {

// Construct a ProtoField with the given tags.
proto::ProtoField ProtoField(std::initializer_list<int32_t> tags);

inline proto::Value StrProto(std::string value) {
  proto::Value v;
  v.set_string_value(std::move(value));
  return v;
}

inline proto::Value Int32Proto(int32_t value) {
  proto::Value v;
  v.set_int32_value(value);
  return v;
}

inline proto::Value Int64Proto(int64_t value) {
  proto::Value v;
  v.set_int64_value(value);
  return v;
}

inline proto::Value BoolProto(bool value) {
  proto::Value v;
  v.set_boolean_value(value);
  return v;
}

inline proto::Condition Eq(proto::ProtoField field, proto::Value value) {
  proto::Condition result;
  *result.mutable_proto_field() = std::move(field);
  result.set_operator_type(proto::OPERATOR_TYPE_EQUAL_TO);
  *result.mutable_value() = std::move(value);
  return result;
}

inline proto::Condition Neq(proto::ProtoField field, proto::Value value) {
  proto::Condition result;
  *result.mutable_proto_field() = std::move(field);
  result.set_operator_type(proto::OPERATOR_TYPE_NOT_EQUAL_TO);
  *result.mutable_value() = std::move(value);
  return result;
}

// Construct AND-joined conditions.
proto::ConditionList All(std::initializer_list<proto::Condition>);

// Construct OR-joined conditions.
proto::ConditionList Any(std::initializer_list<proto::Condition>);

inline proto::StringArg StringArg(std::string raw_string) {
  proto::StringArg result;
  result.set_raw_string(std::move(raw_string));
  return result;
}

inline proto::StringArg StringArg(proto::ProtoField field) {
  proto::StringArg result;
  *result.mutable_proto_field() = std::move(field);
  return result;
}

inline proto::StringArg StringArg(proto::ControlToken token) {
  proto::StringArg result;
  result.set_control_token(token);
  return result;
}

inline proto::StringArg RangeExprArg(proto::ProtoField repeated_field,
                                     proto::SubstitutedString expr) {
  proto::StringArg result;
  *result.mutable_range_expr()->mutable_proto_field() =
      std::move(repeated_field);
  *result.mutable_range_expr()->mutable_expr() = std::move(expr);
  return result;
}

// Construct an IndexExpr in a StringArg.
inline proto::StringArg IndexExprArg(bool one_based) {
  proto::StringArg result;
  result.mutable_index_expr()->set_one_based(one_based);
  return result;
}

// Construct an MediaField in a StringArg.
inline proto::StringArg MediaFieldArg(std::initializer_list<int32_t> tags) {
  proto::StringArg result;
  *result.mutable_media_field()->mutable_proto_field() = ProtoField(tags);
  return result;
}

// Add a condition to a StringArg.
inline proto::StringArg If(proto::ConditionList conditions,
                           proto::StringArg arg) {
  *arg.mutable_conditions() = std::move(conditions);
  return arg;
}

// If(field=value) -> Arg
template <typename Enum>
  requires(std::is_enum_v<Enum>)
proto::StringArg EnumCase(proto::ProtoField field,
                          Enum value,
                          proto::StringArg arg) {
  return If(All({Eq(std::move(field), Int32Proto(value))}), std::move(arg));
}

// A StringSubstitution with only one candidate.
inline proto::StringSubstitution Always(proto::StringArg arg) {
  proto::StringSubstitution result;
  *result.add_candidates() = arg;
  return result;
}

// A SubstitutedString that contains one element.
inline proto::SubstitutedString Just(proto::StringSubstitution sub) {
  proto::SubstitutedString result;
  result.set_string_template("%s");
  *result.add_substitutions() = std::move(sub);
  return result;
}

inline proto::SubstitutedString Just(proto::StringArg arg) {
  return Just(Always(arg));
}

// Constructs a StringSubstitution object with provided candidates.
proto::StringSubstitution Candidates(
    std::initializer_list<proto::StringArg> candidates);

proto::SubstitutedString Concatenated(
    std::initializer_list<proto::StringSubstitution> substitutions);

// Make Substitution putting 'field' in 'tmpl'.
proto::SubstitutedString FieldSubstitution(const std::string& tmpl,
                                           proto::ProtoField field);

// Make a Substitution that formats a repeated field.
proto::SubstitutedString ForEachSubstitution(proto::ProtoField repeated_field,
                                             proto::SubstitutedString expr);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_SUBSTITUTION_BUILDER_H_
