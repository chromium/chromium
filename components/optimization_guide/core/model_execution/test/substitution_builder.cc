// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/substitution_builder.h"

#include "components/optimization_guide/proto/substitution.pb.h"

namespace optimization_guide {

proto::ProtoField ProtoField(std::initializer_list<int32_t> tags) {
  proto::ProtoField f;
  for (int32_t tag : tags) {
    f.add_proto_descriptors()->set_tag_number(tag);
  }
  return f;
}

proto::StringSubstitution Candidates(
    std::initializer_list<proto::StringArg> candidates) {
  proto::StringSubstitution result;
  for (auto& candidate : candidates) {
    *result.add_candidates() = candidate;
  }
  return result;
}

proto::ConditionList All(std::initializer_list<proto::Condition> conditions) {
  proto::ConditionList result;
  result.set_condition_evaluation_type(proto::CONDITION_EVALUATION_TYPE_AND);
  for (auto& condition : conditions) {
    *result.add_conditions() = condition;
  }
  return result;
}

proto::ConditionList Any(std::initializer_list<proto::Condition> conditions) {
  proto::ConditionList result;
  result.set_condition_evaluation_type(proto::CONDITION_EVALUATION_TYPE_OR);
  for (auto& condition : conditions) {
    *result.add_conditions() = condition;
  }
  return result;
}

proto::SubstitutedString FieldSubstitution(const std::string& tmpl,
                                           proto::ProtoField field) {
  proto::SubstitutedString result;
  result.set_string_template(tmpl);
  *result.add_substitutions()->add_candidates()->mutable_proto_field() =
      std::move(field);
  return result;
}

proto::SubstitutedString ForEachSubstitution(proto::ProtoField repeated_field,
                                             proto::SubstitutedString expr) {
  proto::SubstitutedString result;
  result.set_string_template("%s");
  *result.add_substitutions()->add_candidates() =
      RangeExprArg(std::move(repeated_field), std::move(expr));
  return result;
}

}  // namespace optimization_guide
