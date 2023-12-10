// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_rationalization_engine.h"

#include <array>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"

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

namespace internal {
bool IsEnvironmentConditionFulfilled(const EnvironmentCondition& env,
                                     const GeoIpCountryCode& client_country) {
  if (!env.country_list.empty() &&
      !base::Contains(env.country_list, client_country)) {
    return false;
  }

  if (env.feature && !base::FeatureList::IsEnabled(*env.feature)) {
    return false;
  }

  return true;
}

bool IsFieldConditionFulfilledIgnoringLocation(
    const FieldCondition& condition,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    const AutofillField& field) {
  if (condition.possible_overall_types.has_value() &&
      !condition.possible_overall_types->contains(
          field.Type().GetStorableType())) {
    return false;
  }

  if (condition.regex_reference_match.has_value()) {
    base::span<const MatchPatternRef> patterns = GetMatchPatterns(
        condition.regex_reference_match.value(), page_language, pattern_source);
    if (!FormField::FieldMatchesMatchPatternRef(patterns, field)) {
      return false;
    }
  }

  return true;
}

std::optional<size_t> FindFieldMeetingCondition(
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    size_t start_index,
    const FieldCondition& condition,
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  int direction = [&condition]() {
    switch (condition.location) {
      case FieldLocation::kPredecessor:
      case FieldLocation::kLastClassifiedPredecessor:
        return -1;
      case FieldLocation::kTriggerField:
        NOTREACHED_NORETURN();
      case FieldLocation::kNextClassifiedSuccessor:
      case FieldLocation::kSuccessor:
        return 1;
    }
  }();

  for (int i = start_index + direction;
       i >= 0 && i < static_cast<int>(fields.size()); i += direction) {
    const AutofillField& candidate_field = *fields[i];
    if (IsFieldConditionFulfilledIgnoringLocation(
            condition, page_language, pattern_source, candidate_field)) {
      return static_cast<size_t>(i);
    }

    if (candidate_field.Type().GetStorableType() != UNKNOWN_TYPE &&
        (condition.location == FieldLocation::kLastClassifiedPredecessor ||
         condition.location == FieldLocation::kNextClassifiedSuccessor)) {
      // Don't try any further once we have checked the last/next classified
      // field.
      break;
    }
  }

  return std::nullopt;
}

void ApplyRuleIfApplicable(
    const RationalizationRule& rule,
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    LogManager* log_manager) {
  if (rule.environment_condition.has_value() &&
      !IsEnvironmentConditionFulfilled(rule.environment_condition.value(),
                                       client_country)) {
    return;
  }

  for (size_t i = 0; i < fields.size(); ++i) {
    const std::unique_ptr<AutofillField>& trigger_field = fields[i];

    // Check whether we have found a trigger field at index i.
    if (!IsFieldConditionFulfilledIgnoringLocation(
            rule.trigger_field, page_language, pattern_source,
            *trigger_field)) {
      continue;
    }

    // Check whether all the other conditions are also met for the surrounding
    // fields.
    base::flat_map<FieldLocation, size_t> found_fields;
    for (const FieldCondition& other_field_condition :
         rule.other_field_conditions) {
      CHECK_NE(other_field_condition.location, FieldLocation::kTriggerField);
      std::optional<size_t> match_index = FindFieldMeetingCondition(
          fields, i, other_field_condition, client_country, page_language,
          pattern_source);
      if (!match_index.has_value()) {
        break;
      }
      found_fields[other_field_condition.location] = match_index.value();
    }
    // Only proceed if all other conditions were met.
    if (found_fields.size() != rule.other_field_conditions.size()) {
      continue;
    }

    found_fields[FieldLocation::kTriggerField] = i;

    // Apply actions.
    LogBuffer buffer(IsLoggingActive(log_manager));
    for (const SetTypeAction& action : rule.actions) {
      // Actions can only happen for fields that are bound via conditions.
      CHECK(found_fields.find(action.target) != found_fields.end());
      AutofillField& field = *fields[found_fields[action.target]];
      buffer << ", changing field " << found_fields[action.target] << " from "
             << FieldTypeToStringView(field.Type().GetStorableType()) << " to "
             << FieldTypeToStringView(action.set_overall_type);
      field.SetTypeTo(AutofillType(action.set_overall_type));
    }
    LOG_AF(log_manager) << LoggingScope::kRationalization
                        << LogMessage::kRationalization << rule.rule_name
                        << " applies, performing changes: "
                        << std::move(buffer);
  }
}

}  // namespace internal

void ApplyRationalizationEngineRules(
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    LogManager* log_manager) {
  auto create_rules = [] {
    return std::to_array(
        {RationalizationRuleBuilder()
             // A name for the rule (for logging purposes).
             .SetRuleName("Fix colonia as address-line2 in MX")

             // Only if the requirements specified in the environment are all
             // met, the RationalizationRule is executed.
             .SetEnvironmentCondition(
                 EnvironmentConditionBuilder()
                     .SetCountryList({GeoIpCountryCode("MX")})
                     .SetFeature(
                         &features::kAutofillEnableRationalizationEngineForMX)
                     .Build())

             // This is the core field to which the rule applies.
             .SetTriggerField(FieldCondition{
                 // The trigger field needs to be an ADDRESS_HOME_LINE2.
                 .possible_overall_types =
                     ServerFieldTypeSet{ADDRESS_HOME_LINE2},
                 // Lookup in legacy_regex_patterns.
                 .regex_reference_match = "ADDRESS_HOME_DEPENDENT_LOCALITY",
             })

             // All of the following conditions need to be met for actions to be
             // executed. The .location specifies which fields to consider. It
             // also binds the fields to a label that is later referenced in
             // actions.
             .SetOtherFieldConditions({
                 FieldCondition{
                     .location = FieldLocation::kLastClassifiedPredecessor,
                     .possible_overall_types =
                         ServerFieldTypeSet{ADDRESS_HOME_LINE1},
                 },
             })

             // What actions to perform on the trigger fields and other fields
             // that had conditions.
             .SetActions({
                 SetTypeAction{
                     .target = FieldLocation::kLastClassifiedPredecessor,
                     .set_overall_type = ADDRESS_HOME_STREET_ADDRESS,
                 },
                 SetTypeAction{
                     .target = FieldLocation::kTriggerField,
                     .set_overall_type = ADDRESS_HOME_DEPENDENT_LOCALITY,
                 },
             })
             .Build()});
  };
  static const base::NoDestructor<decltype(create_rules())>
      kRationalizationRules(create_rules());

  for (const RationalizationRule& rule : *kRationalizationRules) {
    internal::ApplyRuleIfApplicable(rule, client_country, page_language,
                                    pattern_source, fields, log_manager);
  }
}

}  // namespace autofill::rationalization
