// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_rationalization_engine.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"

namespace autofill::rationalization {

EnvironmentCondition::EnvironmentCondition() = default;
EnvironmentCondition::EnvironmentCondition(EnvironmentCondition&&) = default;
EnvironmentCondition::~EnvironmentCondition() = default;
EnvironmentCondition& EnvironmentCondition::operator=(EnvironmentCondition&&) =
    default;

EnvironmentConditionBuilder::EnvironmentConditionBuilder() = default;
EnvironmentConditionBuilder::~EnvironmentConditionBuilder() = default;

EnvironmentConditionBuilder& EnvironmentConditionBuilder::SetCountryList(
    std::vector<GeoIpCountryCode> country_list) & {
  env_.country_list = std::move(country_list);
  return *this;
}

EnvironmentConditionBuilder&& EnvironmentConditionBuilder::SetCountryList(
    std::vector<GeoIpCountryCode> country_list) && {
  return std::move(this->SetCountryList(std::move(country_list)));
}

EnvironmentConditionBuilder& EnvironmentConditionBuilder::SetFeature(
    const base::Feature* feature) & {
  env_.feature = feature;
  return *this;
}

EnvironmentConditionBuilder&& EnvironmentConditionBuilder::SetFeature(
    const base::Feature* feature) && {
  return std::move(this->SetFeature(feature));
}

EnvironmentCondition EnvironmentConditionBuilder::Build() && {
  return std::move(env_);
}

RationalizationRule::RationalizationRule() = default;
RationalizationRule::~RationalizationRule() = default;

RationalizationRuleBuilder::RationalizationRuleBuilder() = default;
RationalizationRuleBuilder::~RationalizationRuleBuilder() = default;

RationalizationRule::RationalizationRule(RationalizationRule&&) = default;
RationalizationRule& RationalizationRule::operator=(RationalizationRule&&) =
    default;

RationalizationRuleBuilder& RationalizationRuleBuilder::SetRuleName(
    std::string_view rule_name) & {
  rule.rule_name = rule_name;
  return *this;
}
RationalizationRuleBuilder&& RationalizationRuleBuilder::SetRuleName(
    std::string_view rule_name) && {
  return std::move(this->SetRuleName(rule_name));
}

RationalizationRuleBuilder& RationalizationRuleBuilder::SetEnvironmentCondition(
    EnvironmentCondition environment_condition) & {
  rule.environment_condition = std::move(environment_condition);
  return *this;
}
RationalizationRuleBuilder&&
RationalizationRuleBuilder::SetEnvironmentCondition(
    EnvironmentCondition environment_condition) && {
  return std::move(
      this->SetEnvironmentCondition(std::move(environment_condition)));
}

RationalizationRuleBuilder& RationalizationRuleBuilder::SetTriggerField(
    FieldCondition trigger_field) & {
  rule.trigger_field = std::move(trigger_field);
  return *this;
}

RationalizationRuleBuilder&& RationalizationRuleBuilder::SetTriggerField(
    FieldCondition trigger_field) && {
  return std::move(this->SetTriggerField(std::move(trigger_field)));
}

RationalizationRuleBuilder& RationalizationRuleBuilder::SetOtherFieldConditions(
    std::vector<FieldCondition> other_field_conditions) & {
  rule.other_field_conditions = std::move(other_field_conditions);
  return *this;
}
RationalizationRuleBuilder&&
RationalizationRuleBuilder::SetOtherFieldConditions(
    std::vector<FieldCondition> other_field_conditions) && {
  return std::move(
      this->SetOtherFieldConditions(std::move(other_field_conditions)));
}

RationalizationRuleBuilder& RationalizationRuleBuilder::SetActions(
    std::vector<SetTypeAction> actions) & {
  rule.actions = std::move(actions);
  return *this;
}
RationalizationRuleBuilder&& RationalizationRuleBuilder::SetActions(
    std::vector<SetTypeAction> actions) && {
  return std::move(this->SetActions(std::move(actions)));
}

RationalizationRule RationalizationRuleBuilder::Build() && {
  return std::move(rule);
}

}  // namespace autofill::rationalization
