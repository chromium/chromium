// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/configs/string_substitution_builder.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "components/optimization_guide/core/model_execution/configs/substitution_builder.h"

namespace optimization_guide {

StringSubstitutionBuilder::StringSubstitutionBuilder() = default;

StringSubstitutionBuilder::StringSubstitutionBuilder(proto::StringArg arg) {
  Else(std::move(arg));
}

StringSubstitutionBuilder::StringSubstitutionBuilder(std::string raw_string) {
  Else(std::move(raw_string));
}

StringSubstitutionBuilder::StringSubstitutionBuilder(
    proto::StringSubstitution substitution)
    : substitution_(std::move(substitution)) {}

StringSubstitutionBuilder::StringSubstitutionBuilder(
    const StringSubstitutionBuilder&) = default;
StringSubstitutionBuilder& StringSubstitutionBuilder::operator=(
    const StringSubstitutionBuilder&) = default;
StringSubstitutionBuilder::StringSubstitutionBuilder(
    StringSubstitutionBuilder&&) = default;
StringSubstitutionBuilder& StringSubstitutionBuilder::operator=(
    StringSubstitutionBuilder&&) = default;
StringSubstitutionBuilder::~StringSubstitutionBuilder() = default;

StringSubstitutionBuilder& StringSubstitutionBuilder::If(
    proto::Condition condition,
    proto::StringArg arg) {
  return Else(
      optimization_guide::If(All({std::move(condition)}), std::move(arg)));
}

StringSubstitutionBuilder& StringSubstitutionBuilder::If(
    proto::Condition condition,
    std::string raw_string) {
  return If(std::move(condition), StringArg(std::move(raw_string)));
}

StringSubstitutionBuilder& StringSubstitutionBuilder::If(
    proto::Condition condition,
    StringSubstitutionBuilder other) {
  other.PrependCondition(std::move(condition));
  return Else(other);
}

StringSubstitutionBuilder& StringSubstitutionBuilder::If(
    proto::Condition condition,
    proto::StringSubstitution other) {
  return If(std::move(condition), StringSubstitutionBuilder(std::move(other)));
}

StringSubstitutionBuilder& StringSubstitutionBuilder::Else(
    proto::StringArg arg) {
  *substitution_.add_candidates() = std::move(arg);
  return *this;
}

StringSubstitutionBuilder& StringSubstitutionBuilder::Else(
    std::string raw_string) {
  return Else(StringArg(std::move(raw_string)));
}

StringSubstitutionBuilder& StringSubstitutionBuilder::Else(
    const proto::StringSubstitution& other) {
  substitution_.mutable_candidates()->MergeFrom(other.candidates());
  return *this;
}

StringSubstitutionBuilder& StringSubstitutionBuilder::Else(
    const StringSubstitutionBuilder& other) {
  return Else(other.substitution_);
}

StringSubstitutionBuilder& StringSubstitutionBuilder::PrependCondition(
    proto::Condition condition) {
  for (proto::StringArg& candidate : *substitution_.mutable_candidates()) {
    proto::ConditionList new_conditions;
    new_conditions.set_condition_evaluation_type(
        proto::CONDITION_EVALUATION_TYPE_AND);
    *new_conditions.add_conditions() = condition;
    if (candidate.has_conditions() &&
        !candidate.conditions().conditions().empty()) {
      CHECK_EQ(candidate.conditions().condition_evaluation_type(),
               proto::CONDITION_EVALUATION_TYPE_AND);
      new_conditions.mutable_conditions()->MergeFrom(
          candidate.conditions().conditions());
    }
    *candidate.mutable_conditions() = std::move(new_conditions);
  }
  return *this;
}

proto::StringSubstitution StringSubstitutionBuilder::Build() const& {
  return substitution_;
}

proto::StringSubstitution StringSubstitutionBuilder::Build() && {
  return std::move(substitution_);
}

}  // namespace optimization_guide
