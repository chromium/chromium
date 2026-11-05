// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_rationalization_engine.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"

namespace autofill::rationalization {

namespace internal {
bool IsEnvironmentConditionFulfilled(ParsingContext& context,
                                     const EnvironmentCondition& env) {
  if (!env.country_list.empty() &&
      !std::ranges::contains(env.country_list,
                             context.client_country.value())) {
    return false;
  }

  if (env.is_feature_enabled && !env.is_feature_enabled()) {
    return false;
  }

  return true;
}

bool IsFieldConditionFulfilledIgnoringLocation(ParsingContext& context,
                                               const FieldCondition& condition,
                                               const AutofillField& field) {
  if (condition.possible_overall_types.has_value() &&
      !condition.possible_overall_types->contains_any(
          field.Type().GetTypes())) {
    return false;
  }

  if (condition.regex_reference_match.has_value()) {
    if (!FormFieldParser::FieldMatchesMatchPatternRef(
            context, field, condition.regex_reference_match.value())) {
      return false;
    }
  }

  return true;
}

std::optional<size_t> FindFieldMeetingCondition(
    ParsingContext& context,
    base::span<const std::unique_ptr<AutofillField>> fields,
    size_t start_index,
    const FieldCondition& condition) {
  // This function is called with `start_index` pointing to the trigger field.
  // It will search in the direction of `condition.location` for a field that
  // meets the `condition`, and returns whether it found one.
  auto search = [&](int direction) -> std::optional<size_t> {
    for (int i = start_index + direction;
         i >= 0 && i < static_cast<int>(fields.size()); i += direction) {
      const AutofillField& candidate_field = *fields[i];
      if (IsFieldConditionFulfilledIgnoringLocation(context, condition,
                                                    candidate_field)) {
        return static_cast<size_t>(i);
      }
      if (!candidate_field.Type().GetTypes().contains(UNKNOWN_TYPE) &&
          ((direction > 0 &&
            condition.location == FieldLocation::kNextClassifiedSuccessor) ||
           (direction < 0 &&
            condition.location == FieldLocation::kLastClassifiedPredecessor))) {
        // Don't try any further once we have checked the last/next classified
        // field.
        break;
      }
    }
    return std::nullopt;
  };

  // Determine search strategy based on `condition.location`.
  switch (condition.location) {
    case FieldLocation::kPredecessor:
    case FieldLocation::kLastClassifiedPredecessor:
      return search(/*direction=*/-1);  // Backward search.
    case FieldLocation::kTriggerField:
      NOTREACHED();
    case FieldLocation::kNextClassifiedSuccessor:
    case FieldLocation::kSuccessor:
      return search(/*direction=*/1);  // Forward search.
    case FieldLocation::kAnywhere:
      // Try forward first, then backward.
      const std::optional<size_t> forward_result = search(/*direction=*/1);
      return forward_result ? forward_result : search(/*direction=*/-1);
  }
  return std::nullopt;
}

void ApplyRuleIfApplicable(
    ParsingContext& context,
    const RationalizationRule& rule,
    base::span<const std::unique_ptr<AutofillField>> fields,
    LogManager* log_manager) {
  if (!IsEnvironmentConditionFulfilled(context, rule.environment_condition)) {
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
      CHECK(found_fields.contains(action.target));
      AutofillField& field = *fields[found_fields[action.target]];
      buffer << ", changing field " << found_fields[action.target] << " from "
             << FieldTypeSetToString(field.Type().GetTypes()) << " to "
             << FieldTypeToStringView(action.set_overall_type);
      field.SetTypeTo(AutofillType(action.set_overall_type),
                      AutofillPredictionSource::kRationalization);
    }
    LOG_AF(log_manager) << LoggingScope::kRationalization
                        << LogMessage::kRationalization << rule.rule_name
                        << " applies, performing changes: "
                        << std::move(buffer);
  }
}

}  // namespace internal

namespace {

// Helper to define inline `static constexpr` arrays for `base::raw_span`
// fields inside `kRationalizationRules`.
#define STATIC_SPAN(Type, ...)                    \
  ([] {                                           \
    static constexpr Type kArray[] = __VA_ARGS__; \
    return base::span<const Type>(kArray);        \
  }())

constexpr EnvironmentCondition kInAddressModelEnv{
    .country_list = STATIC_SPAN(std::string_view, {"IN"}),
    .is_feature_enabled =
        [] {
          return base::FeatureList::IsEnabled(
              features::kAutofillUseINAddressModel);
        },
};

constexpr std::array kRationalizationRules = std::to_array<
    RationalizationRule>({
    {
        // A name for the rule (for logging purposes).
        .rule_name = "Fix colonia as address-line2 in MX",
        // Only if the requirements specified in the environment are all met,
        // the RationalizationRule is executed.
        .environment_condition = {.country_list =
                                      STATIC_SPAN(std::string_view, {"MX"})},
        // This is the core field to which the rule applies.
        .trigger_field =
            FieldCondition{
                // The trigger field needs to be an ADDRESS_HOME_LINE2.
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE2},
                // Lookup in legacy_regex_patterns.
                .regex_reference_match = "ADDRESS_HOME_DEPENDENT_LOCALITY",
            },
        // All of the following conditions need to be met for actions to be
        // executed. The .location specifies which fields to consider. It also
        // binds the fields to a label that is later referenced in actions.
        .other_field_conditions = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kLastClassifiedPredecessor,
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
            }}),
        // What actions to perform on the trigger fields and other fields that
        // had conditions.
        .actions = STATIC_SPAN(
            SetTypeAction,
            {
                {
                    .target = FieldLocation::kLastClassifiedPredecessor,
                    .set_overall_type = ADDRESS_HOME_STREET_ADDRESS,
                },
                {
                    .target = FieldLocation::kTriggerField,
                    .set_overall_type = ADDRESS_HOME_DEPENDENT_LOCALITY,
                },
            }),
    },
    {
        .rule_name = "Fix address-overflow as address-line2 in DE",
        .environment_condition = {.country_list =
                                      STATIC_SPAN(std::string_view, {"DE"})},
        .trigger_field =
            FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_OVERFLOW},
            },
        .other_field_conditions =
            STATIC_SPAN(FieldCondition,
                        {{
                            .location = FieldLocation::kPredecessor,
                            .possible_overall_types =
                                FieldTypeSet{ADDRESS_HOME_STREET_ADDRESS,
                                             ADDRESS_HOME_STREET_LOCATION,
                                             ADDRESS_HOME_LINE1},
                        }}),
        .actions = STATIC_SPAN(SetTypeAction,
                               {
                                   {
                                       .target = FieldLocation::kPredecessor,
                                       .set_overall_type = ADDRESS_HOME_LINE1,
                                   },
                                   {
                                       .target = FieldLocation::kTriggerField,
                                       .set_overall_type = ADDRESS_HOME_LINE2,
                                   },
                               }),
    },
    {
        .rule_name = "Rationalize ADDRESS_HOME_LINE1 into "
                     "ADDRESS_HOME_STREET_ADDRESS for DE",
        .environment_condition = {.country_list =
                                      STATIC_SPAN(std::string_view, {"DE"})},
        .trigger_field =
            FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
            },
        .fields_with_conditions_do_not_exist = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kSuccessor,
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE2},
            }}),
        .actions =
            STATIC_SPAN(SetTypeAction,
                        {{
                            .target = FieldLocation::kTriggerField,
                            .set_overall_type = ADDRESS_HOME_STREET_ADDRESS,
                        }}),
    },
    {
        .rule_name = "Fix ADDRESS_HOME_HOUSE_NUMBER_AND_APT for PL",
        .environment_condition = {.country_list =
                                      STATIC_SPAN(std::string_view, {"PL"})},
        .trigger_field =
            FieldCondition{
                .possible_overall_types =
                    FieldTypeSet{ADDRESS_HOME_HOUSE_NUMBER},
            },
        .other_field_conditions = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kLastClassifiedPredecessor,
                .possible_overall_types =
                    FieldTypeSet{ADDRESS_HOME_STREET_NAME},
            }}),
        .fields_with_conditions_do_not_exist = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kNextClassifiedSuccessor,
                .possible_overall_types =
                    FieldTypeSet{ADDRESS_HOME_APT_NUM, ADDRESS_HOME_APT_TYPE,
                                 UNKNOWN_TYPE, ADDRESS_HOME_LINE1,
                                 ADDRESS_HOME_LINE2, ADDRESS_HOME_LINE3},
            }}),
        .actions = STATIC_SPAN(
            SetTypeAction,
            {{
                .target = FieldLocation::kTriggerField,
                .set_overall_type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
            }}),
    },
    {
        .rule_name = "Fix ADDRESS_HOME_LINE1 for PL",
        .environment_condition = {.country_list =
                                      STATIC_SPAN(std::string_view, {"PL"})},
        .trigger_field =
            FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
            },
        .fields_with_conditions_do_not_exist = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kNextClassifiedSuccessor,
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE2},
            }}),
        .actions =
            STATIC_SPAN(SetTypeAction,
                        {{
                            .target = FieldLocation::kTriggerField,
                            .set_overall_type = ADDRESS_HOME_STREET_ADDRESS,
                        }}),
    },
    {
        .rule_name = "Fix consecutive ADDRESS_HOME_LINE1 for IT",
        .environment_condition = {.country_list =
                                      STATIC_SPAN(std::string_view, {"IT"})},
        .trigger_field =
            FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
            },
        .other_field_conditions = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kNextClassifiedSuccessor,
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
            }}),
        .actions =
            STATIC_SPAN(SetTypeAction,
                        {{
                            .target = FieldLocation::kNextClassifiedSuccessor,
                            .set_overall_type = ADDRESS_HOME_LINE2,
                        }}),
    },
    {
        .rule_name = "Fix ADDRESS_HOME_LINE1 without following "
                     "ADDRESS_HOME_LINE2 for IT",
        .environment_condition = {.country_list =
                                      STATIC_SPAN(std::string_view, {"IT"})},
        .trigger_field =
            FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
            },
        .fields_with_conditions_do_not_exist =
            STATIC_SPAN(FieldCondition,
                        {{
                            .location = FieldLocation::kNextClassifiedSuccessor,
                            .possible_overall_types =
                                FieldTypeSet{UNKNOWN_TYPE, ADDRESS_HOME_LINE2},
                        }}),
        .actions =
            STATIC_SPAN(SetTypeAction,
                        {{
                            .target = FieldLocation::kTriggerField,
                            .set_overall_type = ADDRESS_HOME_STREET_ADDRESS,
                        }}),
    },
    {
        .rule_name = "Fix ADDRESS_HOME_HOUSE_NUMBER_AND_APT for NL",
        .environment_condition = {.country_list =
                                      STATIC_SPAN(std::string_view, {"NL"})},
        .trigger_field =
            FieldCondition{
                .possible_overall_types =
                    FieldTypeSet{ADDRESS_HOME_HOUSE_NUMBER},
            },
        .fields_with_conditions_do_not_exist = STATIC_SPAN(
            FieldCondition,
            {
                {
                    .location = FieldLocation::kNextClassifiedSuccessor,
                    .possible_overall_types =
                        FieldTypeSet{ADDRESS_HOME_APT_NUM},
                },
                {
                    .location = FieldLocation::kLastClassifiedPredecessor,
                    .possible_overall_types =
                        FieldTypeSet{ADDRESS_HOME_APT_NUM},
                },
            }),
        .actions = STATIC_SPAN(
            SetTypeAction,
            {{
                .target = FieldLocation::kTriggerField,
                .set_overall_type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
            }}),
    },
    {
        .rule_name = "Fix ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY for IN",
        .environment_condition = kInAddressModelEnv,
        .trigger_field =
            FieldCondition{
                .possible_overall_types =
                    FieldTypeSet{ADDRESS_HOME_LINE1,
                                 ADDRESS_HOME_STREET_LOCATION,
                                 ADDRESS_HOME_STREET_ADDRESS},
            },
        .other_field_conditions = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kAnywhere,
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LANDMARK},
            }}),
        .fields_with_conditions_do_not_exist =
            STATIC_SPAN(FieldCondition,
                        {{
                            .location = FieldLocation::kAnywhere,
                            .possible_overall_types =
                                FieldTypeSet{ADDRESS_HOME_DEPENDENT_LOCALITY,
                                             ADDRESS_HOME_LINE2},
                        }}),
        .actions = STATIC_SPAN(
            SetTypeAction,
            {{
                .target = FieldLocation::kTriggerField,
                .set_overall_type = ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY,
            }}),
    },
    {
        .rule_name = "Fix ADDRESS_HOME_STREET_ADDRESS for IN",
        .environment_condition = kInAddressModelEnv,
        .trigger_field =
            FieldCondition{
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
            },
        .fields_with_conditions_do_not_exist = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kAnywhere,
                .possible_overall_types =
                    FieldTypeSet{ADDRESS_HOME_DEPENDENT_LOCALITY,
                                 ADDRESS_HOME_LINE2, ADDRESS_HOME_LANDMARK},
            }}),
        .actions =
            STATIC_SPAN(SetTypeAction,
                        {{
                            .target = FieldLocation::kTriggerField,
                            .set_overall_type = ADDRESS_HOME_STREET_ADDRESS,
                        }}),
    },
    {
        .rule_name =
            "Support ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK for IN",
        .environment_condition = kInAddressModelEnv,
        .trigger_field =
            FieldCondition{
                .possible_overall_types =
                    FieldTypeSet{ADDRESS_HOME_DEPENDENT_LOCALITY,
                                 ADDRESS_HOME_STREET_ADDRESS},
            },
        .other_field_conditions =
            STATIC_SPAN(FieldCondition,
                        {{
                            .location = FieldLocation::kAnywhere,
                            .possible_overall_types =
                                FieldTypeSet{ADDRESS_HOME_STREET_LOCATION},
                        }}),
        .fields_with_conditions_do_not_exist = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kAnywhere,
                .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LANDMARK},
            }}),
        .actions =
            STATIC_SPAN(SetTypeAction,
                        {{
                            .target = FieldLocation::kTriggerField,
                            .set_overall_type =
                                ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK,
                        }}),
    },
    {
        .rule_name = "Support ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK for IN",
        .environment_condition = kInAddressModelEnv,
        .trigger_field =
            FieldCondition{
                .possible_overall_types =
                    FieldTypeSet{ADDRESS_HOME_STREET_ADDRESS,
                                 ADDRESS_HOME_STREET_LOCATION,
                                 ADDRESS_HOME_LINE1},
            },
        .other_field_conditions =
            STATIC_SPAN(FieldCondition,
                        {{
                            .location = FieldLocation::kAnywhere,
                            .possible_overall_types =
                                FieldTypeSet{ADDRESS_HOME_DEPENDENT_LOCALITY},
                        }}),
        .fields_with_conditions_do_not_exist = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kAnywhere,
                .possible_overall_types =
                    FieldTypeSet{ADDRESS_HOME_LANDMARK, ADDRESS_HOME_LINE2},
            }}),
        .actions = STATIC_SPAN(
            SetTypeAction,
            {{
                .target = FieldLocation::kTriggerField,
                .set_overall_type = ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK,
            }}),
    },
    // This rules aim to fix the scenario where both name related fields are
    // detected as regular names:
    // |name last||name first|
    // |name last||name first|->|phonetic family name||phonetic given name|
    // The rules for first and last name are split to support also the rare
    // case where the name first field precedes the name last field.
    {
        .rule_name = "Improve phonetic family names field detection in JP",
        .environment_condition = {.country_list =
                                      STATIC_SPAN(std::string_view, {"JP"})},
        .trigger_field =
            FieldCondition{
                .possible_overall_types =
                    FieldTypeSet{NAME_LAST, NAME_LAST_SECOND},
            },
        .other_field_conditions =
            STATIC_SPAN(FieldCondition,
                        {{
                            .location = FieldLocation::kPredecessor,
                            .possible_overall_types =
                                FieldTypeSet{NAME_LAST, NAME_LAST_SECOND},
                        }}),
        .fields_with_conditions_do_not_exist = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kAnywhere,
                .possible_overall_types = FieldTypeSet{ALTERNATIVE_FAMILY_NAME},
            }}),
        .actions = STATIC_SPAN(SetTypeAction,
                               {{
                                   .target = FieldLocation::kTriggerField,
                                   .set_overall_type = ALTERNATIVE_FAMILY_NAME,
                               }}),
    },
    {
        .rule_name = "Improve phonetic given names field detection in JP",
        .environment_condition = {.country_list =
                                      STATIC_SPAN(std::string_view, {"JP"})},
        .trigger_field =
            FieldCondition{
                .possible_overall_types = FieldTypeSet{NAME_FIRST},
            },
        .other_field_conditions =
            STATIC_SPAN(FieldCondition,
                        {{
                            .location = FieldLocation::kPredecessor,
                            .possible_overall_types = FieldTypeSet{NAME_FIRST},
                        }}),
        .fields_with_conditions_do_not_exist = STATIC_SPAN(
            FieldCondition,
            {{
                .location = FieldLocation::kAnywhere,
                .possible_overall_types = FieldTypeSet{ALTERNATIVE_GIVEN_NAME},
            }}),
        .actions = STATIC_SPAN(SetTypeAction,
                               {{
                                   .target = FieldLocation::kTriggerField,
                                   .set_overall_type = ALTERNATIVE_GIVEN_NAME,
                               }}),
    },
    // This rationalization rule is a client side fix that changes
    // `FieldType` of a field from `LAST_NAME_SECOND` to `LAST_NAME` when
    // `NAME_LAST_FIRST` was not detected.
    // Additionally credit card related `FieldType`s mustn't be observed
    // because sometimes `NAME_LAST_FIRST` is labeled as
    // `CREDIT_CARD_NAME_LAST_FIRST`.
    //
    // NAME_LAST_FIRST and NAME_LAST_SECOND `FieldType`s should appear
    // together because they represent the structure of a Hispanic/Latinx
    // last name.
    {
        .rule_name = "Correct LAST_NAME_SECOND classification to "
                     "LAST_NAME when LAST_NAME_FIRST is absent.",
        .environment_condition =
            {
                .is_feature_enabled =
                    [] {
                      return base::FeatureList::IsEnabled(
                          features::
                              kAutofillImproveClassificationForTwoWordLastNames);
                    },
            },
        .trigger_field =
            FieldCondition{
                .possible_overall_types = FieldTypeSet{NAME_LAST_SECOND},
            },
        .fields_with_conditions_do_not_exist = STATIC_SPAN(
            FieldCondition,
            {
                {
                    .location = FieldLocation::kAnywhere,
                    .possible_overall_types = FieldTypeSet{NAME_LAST_FIRST},
                },
                {
                    .location = FieldLocation::kAnywhere,
                    .possible_overall_types =
                        FieldTypesOfGroup(FieldTypeGroup::kCreditCard),
                },
            }),
        .actions = STATIC_SPAN(SetTypeAction,
                               {{
                                   .target = FieldLocation::kTriggerField,
                                   .set_overall_type = NAME_LAST,
                               }}),
    },
});

#undef STATIC_SPAN

}  // namespace

void ApplyRationalizationEngineRules(
    ParsingContext& context,
    base::span<const std::unique_ptr<AutofillField>> fields,
    LogManager* log_manager) {
  for (const RationalizationRule& rule : kRationalizationRules) {
    internal::ApplyRuleIfApplicable(context, rule, fields, log_manager);
  }
}

}  // namespace autofill::rationalization
