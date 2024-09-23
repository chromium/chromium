// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_RATIONALIZATION_ENGINE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_RATIONALIZATION_ENGINE_H_

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace base {
struct Feature;
}

namespace autofill {
class LogManager;
struct ParsingContext;
}  // namespace autofill

namespace autofill::rationalization {

// Container to hold requirements that need to be all fulfilled for a
// rationalization rule to be applied.
struct EnvironmentCondition {
  EnvironmentCondition();
  ~EnvironmentCondition();
  EnvironmentCondition(EnvironmentCondition&&);
  EnvironmentCondition& operator=(EnvironmentCondition&&);

  // If non-empty, the user needs to be located in a country of the passed
  // list for a rule to apply.
  std::vector<GeoIpCountryCode> country_list;

  // If non-null, the rule is only applied if the feature is enabled.
  raw_ptr<const base::Feature> feature = nullptr;
};

// Builder class to work around Chromium's "Complex class/struct needs an
// explicit out-of-line constructor." code style requirement which prevents the
// use of designated initializer lists.
class EnvironmentConditionBuilder {
 public:
  EnvironmentConditionBuilder();
  ~EnvironmentConditionBuilder();

  EnvironmentConditionBuilder& SetCountryList(
      std::vector<GeoIpCountryCode> country_list) &;
  EnvironmentConditionBuilder&& SetCountryList(
      std::vector<GeoIpCountryCode> country_list) &&;

  EnvironmentConditionBuilder& SetFeature(const base::Feature* feature) &;
  EnvironmentConditionBuilder&& SetFeature(const base::Feature* feature) &&;

  EnvironmentCondition Build() &&;

 private:
  EnvironmentCondition env_;
};

enum class FieldLocation {
  // This specifies a field that needs to precede the trigger field in the
  // form.
  kPredecessor,
  // This specifies the closest predecessor of the trigger field that has a
  // classification that is not UNKNOWN_TYPE.
  kLastClassifiedPredecessor,
  // This refers to the trigger field.
  kTriggerField,
  // This specifies the closest successor of the trigger field that has a
  // classification that is not UNKNOWN_TYPE.
  kNextClassifiedSuccessor,
  // This specifies a field that needs to succeed the trigger field in the
  // form.
  kSuccessor,
};

// Container class for conditions that all need to be true for a field to be
// considered for a rationalization rule.
struct FieldCondition {
  // This specifies the (relative) position to which the condition applies. If
  // it's a directional value (predecessor, successor) instead of the trigger
  // field, the engine will try to find a field to which the conditions match.
  // Each location must be presented only once. This may be relaxed in the
  // future.
  FieldLocation location = FieldLocation::kTriggerField;

  // If specified, the condition is only true if the overall type of the field
  // before the rule evaluation is in `possible_overall_types`.
  std::optional<FieldTypeSet> possible_overall_types;

  // If specified, the condition is only true if the field meets the criteria
  // of the references regular expression. See
  // autofill/core/browser/form_parsing/resources/legacy_regex_patterns.json.
  std::optional<std::string_view> regex_reference_match;
};

// Container that defines what should happen to fields matched for a
// rationalization rule.
struct SetTypeAction {
  // Which field shall be modified. Fields are bound to locations via the
  // conditions.
  FieldLocation target;

  // The new field type to assign to the target.
  FieldType set_overall_type;
};

// A declarative rule with conditions and actions. The actions are executed by
// the rationalization engine if all conditions are fulfilled.
struct RationalizationRule {
  RationalizationRule();
  ~RationalizationRule();
  RationalizationRule(RationalizationRule&&);
  RationalizationRule& operator=(RationalizationRule&&);

  // A name for the rule (for logging purposes).
  std::string_view rule_name;

  // Only if all requirements specified in the environment_condition are
  // fulfilled, the RationalizationRule can be executed.
  std::optional<EnvironmentCondition> environment_condition;

  // Conditions of a rule are anchored on the trigger_field and can be extended
  // by conditions on neighboring fields. This variable specifies the conditions
  // that need to be fulfilled for a trigger field.
  FieldCondition trigger_field;

  // This specifies conditions on the fields following or preceding the
  // the trigger field.
  std::vector<FieldCondition> other_field_conditions;

  // This specifies conditions on the fields following or preceding the
  // the trigger field that should not be met.
  std::vector<FieldCondition> fields_with_conditions_do_not_exist;

  // What rationaliation to apply.
  std::vector<SetTypeAction> actions;
};

// Builder class to work around Chromium's "Complex class/struct needs an
// explicit out-of-line constructor." code style requirement which prevents the
// use of designated initializer lists.
class RationalizationRuleBuilder {
 public:
  RationalizationRuleBuilder();
  ~RationalizationRuleBuilder();

  RationalizationRuleBuilder& SetRuleName(std::string_view rule_name) &;
  RationalizationRuleBuilder&& SetRuleName(std::string_view rule_name) &&;

  RationalizationRuleBuilder& SetEnvironmentCondition(
      EnvironmentCondition environment_condition) &;
  RationalizationRuleBuilder&& SetEnvironmentCondition(
      EnvironmentCondition environment_condition) &&;

  RationalizationRuleBuilder& SetTriggerField(FieldCondition trigger_field) &;
  RationalizationRuleBuilder&& SetTriggerField(FieldCondition trigger_field) &&;

  RationalizationRuleBuilder& SetOtherFieldConditions(
      std::vector<FieldCondition> other_field_conditions) &;
  RationalizationRuleBuilder&& SetOtherFieldConditions(
      std::vector<FieldCondition> other_field_conditions) &&;

  RationalizationRuleBuilder& SetFieldsWithConditionsDoNotExist(
      std::vector<FieldCondition> fields_with_conditions_do_not_exist) &;
  RationalizationRuleBuilder&& SetFieldsWithConditionsDoNotExist(
      std::vector<FieldCondition> fields_with_conditions_do_not_exist) &&;

  RationalizationRuleBuilder& SetActions(std::vector<SetTypeAction> actions) &;
  RationalizationRuleBuilder&& SetActions(
      std::vector<SetTypeAction> actions) &&;

  RationalizationRule Build() &&;

 private:
  RationalizationRule rule;
};

// This is only exposed for testing purposes.
namespace internal {
bool IsEnvironmentConditionFulfilled(ParsingContext& context,
                                     const EnvironmentCondition& env);

bool IsFieldConditionFulfilledIgnoringLocation(ParsingContext& context,
                                               const FieldCondition& condition,
                                               const AutofillField& field);

// Returns the first index of a field in `fields` that meets the `condition`
// when starting at `start_index` and walking in the direction of
// `condition.location`. Returns std::nullopt if no such field exists.
std::optional<size_t> FindFieldMeetingCondition(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    size_t start_index,
    const FieldCondition& condition);

// Performs rationalization according `rule` if the the conditions of the
// rule are met. The `rule` can be executed multiple times on the `fields`.
// Note that the `fields` vector is const but the fields are mutable. This
// constness is inherited from the calling sites.
void ApplyRuleIfApplicable(
    ParsingContext& context,
    const RationalizationRule& rule,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    LogManager* log_manager = nullptr);

}  // namespace internal

// Applies a set of `RationalizationRule`s, which are defined in the function
// body.
// Note that the `fields` vector is const but the fields are mutable. This
// constness is inherited from the calling sites.
void ApplyRationalizationEngineRules(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    LogManager* log_manager = nullptr);

}  // namespace autofill::rationalization

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_RATIONALIZATION_ENGINE_H_
