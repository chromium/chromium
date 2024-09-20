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
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
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

RationalizationRuleBuilder&
RationalizationRuleBuilder::SetFieldsWithConditionsDoNotExist(
    std::vector<FieldCondition> fields_with_conditions_do_not_exist) & {
  rule.fields_with_conditions_do_not_exist =
      std::move(fields_with_conditions_do_not_exist);
  return *this;
}
RationalizationRuleBuilder&&
RationalizationRuleBuilder::SetFieldsWithConditionsDoNotExist(
    std::vector<FieldCondition> fields_with_conditions_do_not_exist) && {
  return std::move(this->SetFieldsWithConditionsDoNotExist(
      std::move(fields_with_conditions_do_not_exist)));
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
bool IsEnvironmentConditionFulfilled(ParsingContext& context,
                                     const EnvironmentCondition& env) {
  if (!env.country_list.empty() &&
      !base::Contains(env.country_list, context.client_country)) {
    return false;
  }

  if (env.feature && !base::FeatureList::IsEnabled(*env.feature)) {
    return false;
  }

  return true;
}

bool IsFieldConditionFulfilledIgnoringLocation(ParsingContext& context,
                                               const FieldCondition& condition,
                                               const AutofillField& field) {
  if (condition.possible_overall_types.has_value() &&
      !condition.possible_overall_types->contains(
          field.Type().GetStorableType())) {
    return false;
  }

  if (condition.regex_reference_match.has_value()) {
    base::span<const MatchPatternRef> patterns =
        GetMatchPatterns(condition.regex_reference_match.value(),
                         context.page_language, context.pattern_file);
    if (!FormFieldParser::FieldMatchesMatchPatternRef(context, patterns,
                                                      field)) {
      return false;
    }
  }

  return true;
}

std::optional<size_t> FindFieldMeetingCondition(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    size_t start_index,
    const FieldCondition& condition) {
  int direction = [&condition]() {
    switch (condition.location) {
      case FieldLocation::kPredecessor:
      case FieldLocation::kLastClassifiedPredecessor:
        return -1;
      case FieldLocation::kTriggerField:
        NOTREACHED();
      case FieldLocation::kNextClassifiedSuccessor:
      case FieldLocation::kSuccessor:
        return 1;
    }
  }();

  for (int i = start_index + direction;
       i >= 0 && i < static_cast<int>(fields.size()); i += direction) {
    const AutofillField& candidate_field = *fields[i];
    if (IsFieldConditionFulfilledIgnoringLocation(context, condition,
                                                  candidate_field)) {
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
    ParsingContext& context,
    const RationalizationRule& rule,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    LogManager* log_manager) {
  if (rule.environment_condition.has_value() &&
      !IsEnvironmentConditionFulfilled(context,
                                       rule.environment_condition.value())) {
    return;
  }

  for (size_t i = 0; i < fields.size(); ++i) {
    const std::unique_ptr<AutofillField>& trigger_field = fields[i];

    // Check whether we have found a trigger field at index i.
    if (!IsFieldConditionFulfilledIgnoringLocation(context, rule.trigger_field,
                                                   *trigger_field)) {
      continue;
    }

    // Check whether all the other conditions are also met for the surrounding
    // fields.
    base::flat_map<FieldLocation, size_t> found_fields;
    for (const FieldCondition& other_field_condition :
         rule.other_field_conditions) {
      CHECK_NE(other_field_condition.location, FieldLocation::kTriggerField);
      std::optional<size_t> match_index =
          FindFieldMeetingCondition(context, fields, i, other_field_condition);
      if (!match_index.has_value()) {
        break;
      }
      found_fields[other_field_condition.location] = match_index.value();
    }
    // Only proceed if all other conditions were met.
    if (found_fields.size() != rule.other_field_conditions.size()) {
      continue;
    }

    bool found_field_with_condition_that_must_not_exist = false;
    for (const FieldCondition& field_with_condition_do_not_exist :
         rule.fields_with_conditions_do_not_exist) {
      CHECK_NE(field_with_condition_do_not_exist.location,
               FieldLocation::kTriggerField);
      if (FindFieldMeetingCondition(context, fields, i,
                                    field_with_condition_do_not_exist)) {
        found_field_with_condition_that_must_not_exist = true;
        break;
      }
    }
    // Don't proceed if the condition which shouldn't be met was satisfied.
    if (found_field_with_condition_that_must_not_exist) {
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
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    LogManager* log_manager) {
  auto create_rules = [] {
    return std::to_array({
        RationalizationRuleBuilder()
            // A name for the rule (for logging purposes).
            .SetRuleName("Fix colonia as address-line2 in MX")

            // Only if the requirements specified in the environment are all
            // met, the RationalizationRule is executed.
            .SetEnvironmentCondition(
                EnvironmentConditionBuilder()
                    .SetCountryList({GeoIpCountryCode("MX")})
                    .Build())

            // This is the core field to which the rule applies.
            .SetTriggerField(FieldCondition{
                // The trigger field needs to be an ADDRESS_HOME_LINE2.
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE2},
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
                    .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
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
            .Build(),
        RationalizationRuleBuilder()
            .SetRuleName("Fix address-overflow as address-line2 in DE")
            .SetEnvironmentCondition(
                EnvironmentConditionBuilder()
                    .SetCountryList({GeoIpCountryCode("DE")})
                    .SetFeature(&features::kAutofillUseDEAddressModel)
                    .Build())
            .SetTriggerField(FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_OVERFLOW}})
            .SetOtherFieldConditions({
                FieldCondition{
                    .location = FieldLocation::kPredecessor,
                    .possible_overall_types =
                        FieldTypeSet{ADDRESS_HOME_STREET_ADDRESS,
                                     ADDRESS_HOME_STREET_LOCATION,
                                     ADDRESS_HOME_LINE1},
                },
            })
            .SetActions({
                SetTypeAction{
                    .target = FieldLocation::kPredecessor,
                    .set_overall_type = ADDRESS_HOME_LINE1,
                },
                SetTypeAction{
                    .target = FieldLocation::kTriggerField,
                    .set_overall_type = ADDRESS_HOME_LINE2,
                },
            })
            .Build(),
        RationalizationRuleBuilder()
            .SetRuleName("Rationalize ADDRESS_HOME_LINE1 into "
                         "ADDRESS_HOME_STREET_ADDRESS for DE")
            .SetEnvironmentCondition(
                EnvironmentConditionBuilder()
                    .SetCountryList({GeoIpCountryCode("DE")})
                    .SetFeature(&features::kAutofillUseDEAddressModel)
                    .Build())
            .SetTriggerField(FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1}})
            .SetFieldsWithConditionsDoNotExist({
                FieldCondition{
                    .location = FieldLocation::kSuccessor,
                    .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE2}},
            })
            .SetActions({
                SetTypeAction{
                    .target = FieldLocation::kTriggerField,
                    .set_overall_type = ADDRESS_HOME_STREET_ADDRESS,
                },
            })
            .Build(),
        RationalizationRuleBuilder()
            .SetRuleName("Fix ADDRESS_HOME_HOUSE_NUMBER_AND_APT for PL")
            .SetEnvironmentCondition(
                EnvironmentConditionBuilder()
                    .SetCountryList({GeoIpCountryCode("PL")})
                    .SetFeature(&features::kAutofillUsePLAddressModel)
                    .Build())
            .SetTriggerField(
                FieldCondition{.possible_overall_types =
                                   FieldTypeSet{ADDRESS_HOME_HOUSE_NUMBER}})
            .SetOtherFieldConditions({
                FieldCondition{
                    .location = FieldLocation::kLastClassifiedPredecessor,
                    .possible_overall_types =
                        FieldTypeSet{ADDRESS_HOME_STREET_NAME},
                },
            })
            .SetFieldsWithConditionsDoNotExist({
                FieldCondition{
                    .location = FieldLocation::kNextClassifiedSuccessor,
                    .possible_overall_types =
                        FieldTypeSet{ADDRESS_HOME_APT_NUM,
                                     ADDRESS_HOME_APT_TYPE, UNKNOWN_TYPE,
                                     ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
                                     ADDRESS_HOME_LINE3}},
            })
            .SetActions({
                SetTypeAction{
                    .target = FieldLocation::kTriggerField,
                    .set_overall_type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
                },
            })
            .Build(),
        RationalizationRuleBuilder()
            .SetRuleName("Fix ADDRESS_HOME_LINE1 for PL")
            .SetEnvironmentCondition(
                EnvironmentConditionBuilder()
                    .SetCountryList({GeoIpCountryCode("PL")})
                    .SetFeature(&features::kAutofillUsePLAddressModel)
                    .Build())
            .SetTriggerField(FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1}})
            .SetFieldsWithConditionsDoNotExist({
                FieldCondition{
                    .location = FieldLocation::kNextClassifiedSuccessor,
                    .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE2}},
            })
            .SetActions({
                SetTypeAction{
                    .target = FieldLocation::kTriggerField,
                    .set_overall_type = ADDRESS_HOME_STREET_ADDRESS,
                },
            })
            .Build(),
        RationalizationRuleBuilder()
            .SetRuleName("Fix consecutive ADDRESS_HOME_LINE1 for IT")
            .SetEnvironmentCondition(
                EnvironmentConditionBuilder()
                    .SetCountryList({GeoIpCountryCode("IT")})
                    .SetFeature(&features::kAutofillUseITAddressModel)
                    .Build())
            .SetTriggerField(FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1}})
            .SetOtherFieldConditions({
                FieldCondition{
                    .location = FieldLocation::kNextClassifiedSuccessor,
                    .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
                },
            })
            .SetActions({
                SetTypeAction{
                    .target = FieldLocation::kNextClassifiedSuccessor,
                    .set_overall_type = ADDRESS_HOME_LINE2,
                },
            })
            .Build(),
        RationalizationRuleBuilder()
            .SetRuleName("Fix ADDRESS_HOME_LINE1 without following "
                         "ADDRESS_HOME_LINE2 for IT")
            .SetEnvironmentCondition(
                EnvironmentConditionBuilder()
                    .SetCountryList({GeoIpCountryCode("IT")})
                    .SetFeature(&features::kAutofillUseITAddressModel)
                    .Build())
            .SetTriggerField(FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1}})
            .SetFieldsWithConditionsDoNotExist({
                FieldCondition{
                    .location = FieldLocation::kNextClassifiedSuccessor,
                    .possible_overall_types =
                        FieldTypeSet{UNKNOWN_TYPE, ADDRESS_HOME_LINE2}},
            })
            .SetActions({
                SetTypeAction{
                    .target = FieldLocation::kTriggerField,
                    .set_overall_type = ADDRESS_HOME_STREET_ADDRESS,
                },
            })
            .Build(),
    });
  };
  static const base::NoDestructor<decltype(create_rules())>
      kRationalizationRules(create_rules());

  for (const RationalizationRule& rule : *kRationalizationRules) {
    internal::ApplyRuleIfApplicable(context, rule, fields, log_manager);
  }
}

}  // namespace autofill::rationalization
