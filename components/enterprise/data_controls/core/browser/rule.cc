// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/rule.h"

#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/data_controls/core/browser/conditions/and_condition.h"
#include "components/enterprise/data_controls/core/browser/conditions/attributes_condition.h"
#include "components/enterprise/data_controls/core/browser/conditions/not_condition.h"
#include "components/enterprise/data_controls/core/browser/conditions/or_condition.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/strings/grit/components_strings.h"

namespace data_controls {
namespace {

// Constants used to parse sub-dictionaries of DLP policies that should map to
// an AttributesCondition.
constexpr char kKeyName[] = "name";
constexpr char kKeyRuleId[] = "rule_id";
constexpr char kKeyDescription[] = "description";
constexpr char kKeySources[] = "sources";
constexpr char kKeyDestinations[] = "destinations";
constexpr char kKeyRestrictions[] = "restrictions";
constexpr char kKeyAnd[] = "and";
constexpr char kKeyOr[] = "or";
constexpr char kKeyNot[] = "not";
constexpr char kKeyClass[] = "class";
constexpr char kKeyLevel[] = "level";

// Helper to make dictionary parsing code more readable.
std::string GetStringOrEmpty(const base::Value::Dict& dict, const char* key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

// A oneof attribute is an attribute that needs to be the only condition in
// their dictionary. If other attributes are present alongside them, it creates
// ambiguity as to how the rule is evaluated, and as such this is considered an
// error in the set policy.
std::vector<std::string_view> OneOfConditions(const base::Value::Dict& value) {
  std::vector<std::string_view> oneof_conditions;
  for (const char* oneof_value :
       {// "and", "or" and "not" need to be the only value at their level as it
        // is otherwise ambiguous which of them has precedence or how they are
        // combined together into one condition.
        kKeyAnd, kKeyOr, kKeyNot,

        // "os_clipboard" needs to be the only value in its dictionary as it
        // represents a unique source/destination. For example, a clipboard
        // interaction cannot both be the OS clipboard and match URL patterns
        // at the same time.
        AttributesCondition::kKeyOsClipboard}) {
    if (value.contains(oneof_value)) {
      oneof_conditions.push_back(oneof_value);
    }
  }
  return oneof_conditions;
}

// Returns any condition present in `value` that wouldn't match
// `OneOfConditions`.
std::vector<std::string_view> AnyOfConditions(const base::Value::Dict& value) {
  std::vector<std::string_view> anyof_conditions;
  for (const char* anyof_condition :
       {kKeySources, kKeyDestinations, AttributesCondition::kKeyUrls,
        AttributesCondition::kKeyIncognito,
        AttributesCondition::kKeyOtherProfile,
#if BUILDFLAG(IS_CHROMEOS)
        AttributesCondition::kKeyComponents
#endif  // BUILDFLAG(IS_CHROMEOS)
       }) {
    if (value.contains(anyof_condition)) {
      anyof_conditions.push_back(anyof_condition);
    }
  }
  return anyof_conditions;
}

// Clones `error_path` and update the copy with a new value.
policy::PolicyErrorPath CreateErrorPath(
    const policy::PolicyErrorPath& error_path,
    std::string new_value,
    std::optional<int> new_list_index = std::nullopt) {
  policy::PolicyErrorPath new_error_path(error_path);
  new_error_path.push_back(std::move(new_value));
  if (new_list_index) {
    new_error_path.push_back(*new_list_index);
  }
  return new_error_path;
}

// Helper to check if a restriction is allowed to be applied to a rule given
// the currently enabled features.
bool IgnoreRestriction(Rule::Restriction restriction) {
  if (restriction == Rule::Restriction::kScreenshot) {
    return !base::FeatureList::IsEnabled(kEnableScreenshotProtection);
  }
  return !base::FeatureList::IsEnabled(kEnableDesktopDataControls);
}

}  // namespace

Rule::Rule(Rule&& other) = default;
Rule::~Rule() = default;

Rule::Rule(std::string name,
           std::string rule_id,
           std::string description,
           std::unique_ptr<const Condition> condition,
           base::flat_map<Restriction, Level> restrictions)
    : name_(std::move(name)),
      rule_id_(std::move(rule_id)),
      description_(std::move(description)),
      condition_(std::move(condition)),
      restrictions_(std::move(restrictions)) {}

// static
std::optional<Rule> Rule::Create(const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }
  return Create(value.GetDict());
}

// static
std::optional<Rule> Rule::Create(const base::Value::Dict& value) {
  auto condition = GetCondition(value);
  if (!condition) {
    return std::nullopt;
  }

  auto restrictions = GetRestrictions(value);

  // To avoid create a `Rule` object with restrictions for disabled features,
  // `restrictions` is first filtered. This is done here instead of in
  // `GetRestrictions()` so that it can still be used by policy handler code to
  // parse the policy correctly.
  base::EraseIf(restrictions, [](const auto& entry) {
    return IgnoreRestriction(entry.first);
  });

  if (restrictions.empty()) {
    return std::nullopt;
  }

  return std::make_optional(Rule(
      GetStringOrEmpty(value, kKeyName), GetStringOrEmpty(value, kKeyRuleId),
      GetStringOrEmpty(value, kKeyDescription), std::move(condition),
      std::move(restrictions)));
}

Rule::Level Rule::GetLevel(Restriction restriction,
                           const ActionContext& context) const {
  // Evaluating the condition of a rule could be expensive, so check
  // preemptively if there are any restrictions first.
  if (!restrictions_.contains(restriction)) {
    return Level::kNotSet;
  }

  if (condition_->CanBeEvaluated(context) && condition_->IsTriggered(context)) {
    return restrictions_.at(restriction);
  }

  return Level::kNotSet;
}

const std::string& Rule::name() const {
  return name_;
}

const std::string& Rule::rule_id() const {
  return rule_id_;
}

const std::string& Rule::description() const {
  return description_;
}

// static
std::unique_ptr<const Condition> Rule::GetCondition(
    const base::Value::Dict& value) {
  // `value` can hold different condition-related keys, namely:
  // - The "not" key with a sub-dict that is parsed recursively.
  // - The "and" or "not" keys with sub-arrays of conditions parsed recursively.
  // - The "sources" and/or "destinations" keys with corresponding sub-dicts.
  // These 3 cases are mutually exclusive and should preemptively be filtered
  // by policy validations of the DataControlsRules policy, and as such the
  // precedence in which these keys are parsed here should not matter.

  if (const base::Value::Dict* condition = value.FindDict(kKeyNot)) {
    return NotCondition::Create(GetCondition(*condition));
  }

  if (const base::Value::List* condition = value.FindList(kKeyAnd)) {
    return AndCondition::Create(GetListConditions(*condition));
  }

  if (const base::Value::List* condition = value.FindList(kKeyOr)) {
    return OrCondition::Create(GetListConditions(*condition));
  }

  // Reaching this statement implies `value` contains no boolean-logic keys
  // ("not", "and", "or") and that it should simply be evaluated based on
  // attributes keys ("sources", "destinations").
  return GetSourcesAndDestinationsCondition(value);
}

// static
std::unique_ptr<const Condition> Rule::GetSourcesAndDestinationsCondition(
    const base::Value::Dict& value) {
  // This function will add a `Condition` for each of the following keys found
  // in `value`:
  // - "sources"
  // - "destinations"
  // Then combine them into an `AndCondition` to make an overall condition for
  // the rule being constructed.
  std::vector<std::unique_ptr<const Condition>> conditions;

  const base::Value* sources_value = value.Find(kKeySources);
  if (sources_value) {
    std::unique_ptr<Condition> sources_condition =
        SourceAttributesCondition::Create(*sources_value);
    if (sources_condition) {
      conditions.push_back(std::move(sources_condition));
    }
  }

  const base::Value* destinations_value = value.Find(kKeyDestinations);
  if (destinations_value) {
    std::unique_ptr<Condition> destinations_condition =
        DestinationAttributesCondition::Create(*destinations_value);
    if (destinations_condition) {
      conditions.push_back(std::move(destinations_condition));
    }
  }

  return AndCondition::Create(std::move(conditions));
}

// static
std::vector<std::unique_ptr<const Condition>> Rule::GetListConditions(
    const base::Value::List& value) {
  std::vector<std::unique_ptr<const Condition>> sub_conditions;
  for (const base::Value& sub_value : value) {
    if (!sub_value.is_dict()) {
      continue;
    }

    auto sub_condition = GetCondition(sub_value.GetDict());
    if (sub_condition) {
      sub_conditions.push_back(std::move(sub_condition));
    }
  }
  return sub_conditions;
}

// static
base::flat_map<Rule::Restriction, Rule::Level> Rule::GetRestrictions(
    const base::Value::Dict& value) {
  const base::Value::List* restrictions_list = value.FindList(kKeyRestrictions);
  if (!restrictions_list) {
    return {};
  }

  base::flat_map<Restriction, Level> restrictions;
  // `restrictions_list` should contain dictionaries of the following schema:
  // {
  //   class: CLIPBOARD|SCREENSHOT|PRINTING|PRIVACY_SCREEN|etc,
  //   level: ALLOW|BLOCK|REPORT|WARN
  // }
  // For compatibility, just ignore unrecognized values and keep iterating and
  // populating `restrictions`.
  for (const base::Value& entry : *restrictions_list) {
    if (!entry.is_dict()) {
      continue;
    }

    const base::Value::Dict& entry_dict = entry.GetDict();
    const std::string* class_string = entry_dict.FindString(kKeyClass);
    const std::string* level_string = entry_dict.FindString(kKeyLevel);
    if (!class_string || !level_string) {
      continue;
    }

    Restriction restriction = StringToRestriction(*class_string);
    Level level = StringToLevel(*level_string);
    if (restriction == Restriction::kUnknownRestriction ||
        level == Level::kNotSet) {
      continue;
    }

    // If there is already an entry for `restriction`, only override it if our
    // current `level` has precedence.
    if (!restrictions.contains(restriction) ||
        restrictions.at(restriction) < level) {
      restrictions[restriction] = level;
    }
  }

  return restrictions;
}

// static
Rule::Restriction Rule::StringToRestriction(const std::string& restriction) {
  static constexpr auto kMap =
      base::MakeFixedFlatMap<std::string_view, Restriction>({
          {kRestrictionClipboard, Restriction::kClipboard},
          {kRestrictionScreenshot, Restriction::kScreenshot},
          {kRestrictionPrinting, Restriction::kPrinting},
          {kRestrictionPrivacyScreen, Restriction::kPrivacyScreen},
          {kRestrictionScreenShare, Restriction::kScreenShare},
          {kRestrictionFiles, Restriction::kFiles},
      });

  static_assert(
      static_cast<int>(Restriction::kMaxValue) == kMap.size(),
      "The Restriction enum needs to have an equivalent string for each value");

  if (!kMap.contains(restriction)) {
    return Restriction::kUnknownRestriction;
  }

  return kMap.at(restriction);
}

// static
Rule::Level Rule::StringToLevel(const std::string& level) {
  static constexpr auto kMap = base::MakeFixedFlatMap<std::string_view, Level>({
      {kLevelAllow, Level::kAllow},
      {kLevelBlock, Level::kBlock},
      {kLevelWarn, Level::kWarn},
      {kLevelReport, Level::kReport},
  });

  static_assert(
      static_cast<int>(Level::kMaxValue) == kMap.size(),
      "The Level enum needs to have an equivalent string for each value");

  if (!kMap.contains(level)) {
    return Level::kNotSet;
  }

  return kMap.at(level);
}

// static
const char* Rule::RestrictionToString(Restriction restriction) {
  // A switch statement is used here instead of a map so that new values being
  // added to the `Restriction` enum break compilation and force updating this
  // code.
  switch (restriction) {
    case Restriction::kUnknownRestriction:
      return nullptr;
    case Restriction::kClipboard:
      return kRestrictionClipboard;
    case Restriction::kScreenshot:
      return kRestrictionScreenshot;
    case Restriction::kPrinting:
      return kRestrictionPrinting;
    case Restriction::kPrivacyScreen:
      return kRestrictionPrivacyScreen;
    case Restriction::kScreenShare:
      return kRestrictionScreenShare;
    case Restriction::kFiles:
      return kRestrictionFiles;
  }
}

// static
const char* Rule::LevelToString(Level level) {
  // A switch statement is used here instead of a map so that new values being
  // added to the `Level` enum break compilation and force updating this code.
  switch (level) {
    case Level::kNotSet:
      return nullptr;
    case Level::kAllow:
      return kLevelAllow;
    case Level::kBlock:
      return kLevelBlock;
    case Level::kWarn:
      return kLevelWarn;
    case Level::kReport:
      return kLevelReport;
  }
}

// static
bool Rule::ValidateRuleValue(const char* policy_name,
                             const base::Value::Dict& root_value,
                             policy::PolicyErrorPath error_path,
                             policy::PolicyErrorMap* errors) {
  auto restrictions = GetRestrictions(root_value);

  if (!AddUnsupportedRestrictionErrors(policy_name, restrictions, error_path,
                                       errors) ||
      restrictions.empty()) {
    return false;
  }

  return ValidateRuleSubValues(policy_name, root_value, restrictions,
                               error_path, errors);
}

// static
bool Rule::ValidateRuleSubValues(
    const char* policy_name,
    const base::Value::Dict& value,
    const base::flat_map<Rule::Restriction, Rule::Level>& restrictions,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  std::vector<std::string_view> oneof_conditions = OneOfConditions(value);
  std::vector<std::string_view> anyof_conditions = AnyOfConditions(value);
  if (oneof_conditions.size() > 1 ||
      (oneof_conditions.size() == 1 && anyof_conditions.size() != 0)) {
    AddMutuallyExclusiveErrors(oneof_conditions, anyof_conditions, policy_name,
                               std::move(error_path), errors);
    return false;
  }

  if (!AddUnsupportedAttributeErrors(oneof_conditions, anyof_conditions,
                                     restrictions, policy_name, error_path,
                                     errors)) {
    return false;
  }

  // Even if the values in `oneof_conditions` and `anyof_conditions` are
  // acceptable for `value`, it's possible there are errors in nested values, so
  // additional checks must be performed recursively.

  bool valid = true;
  for (const char* sub_key : {kKeySources, kKeyDestinations, kKeyNot}) {
    if (value.contains(sub_key)) {
      valid &= ValidateRuleSubValues(
          policy_name, *value.FindDict(sub_key), restrictions,
          CreateErrorPath(error_path, sub_key), errors);
    }
  }
  for (const char* sub_key : {kKeyAnd, kKeyOr}) {
    if (value.contains(sub_key)) {
      int index = 0;
      for (const base::Value& sub_condition : *value.FindList(sub_key)) {
        valid &= ValidateRuleSubValues(
            policy_name, sub_condition.GetDict(), restrictions,
            CreateErrorPath(error_path, sub_key, index), errors);
        ++index;
      }
    }
  }

  return valid;
}

// static
void Rule::AddMutuallyExclusiveErrors(
    const std::vector<std::string_view>& oneof_conditions,
    const std::vector<std::string_view>& anyof_conditions,
    const char* policy_name,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  if (!errors || oneof_conditions.size() == 0) {
    return;
  }

  if (oneof_conditions.size() > 1) {
    errors->AddError(policy_name,
                     IDS_POLICY_DATA_CONTROLS_MUTUALLY_EXCLUSIVE_KEYS,
                     base::JoinString(oneof_conditions, ", "), error_path);
  }

  if (anyof_conditions.size() > 0) {
    errors->AddError(policy_name,
                     IDS_POLICY_DATA_CONTROLS_MUTUALLY_EXCLUSIVE_KEY_SETS,
                     base::JoinString(anyof_conditions, ", "),
                     base::JoinString(oneof_conditions, ", "), error_path);
  }
}

// static
bool Rule::AddUnsupportedAttributeErrors(
    const std::vector<std::string_view>& oneof_conditions,
    const std::vector<std::string_view>& anyof_conditions,
    base::flat_map<Rule::Restriction, Rule::Level> restrictions,
    const char* policy_name,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  static const base::flat_map<Rule::Restriction, std::set<std::string_view>>
      kSupportedAttributes = {
          {Restriction::kClipboard,
           {AttributesCondition::kKeyOsClipboard, AttributesCondition::kKeyUrls,
            AttributesCondition::kKeyIncognito,
            AttributesCondition::kKeyOtherProfile,
#if BUILDFLAG(IS_CHROMEOS)
            AttributesCondition::kKeyComponents,
#endif  // BUILDFLAG(IS_CHROMEOS)
            kKeyAnd, kKeyOr, kKeyNot, kKeySources, kKeyDestinations}},
          {Restriction::kScreenshot,
           {AttributesCondition::kKeyUrls, AttributesCondition::kKeyIncognito,
#if BUILDFLAG(IS_CHROMEOS)
            AttributesCondition::kKeyComponents,
#endif  // BUILDFLAG(IS_CHROMEOS)
            kKeyAnd, kKeyOr, kKeyNot, kKeySources}},
      };

  bool valid = true;
  for (const auto& restriction : restrictions) {
    if (!kSupportedAttributes.contains(restriction.first)) {
      // This shouldn't be reached as `AddUnsupportedRestrictionErrors` should
      // catch these unsupported restrictions.
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    for (const auto& attribute : anyof_conditions) {
      if (!kSupportedAttributes.at(restriction.first).contains(attribute)) {
        if (errors) {
          errors->AddError(policy_name,
                           IDS_POLICY_DATA_CONTROLS_UNSUPPORTED_CONDITION,
                           std::string(attribute),
                           RestrictionToString(restriction.first), error_path);
        }
        valid = false;
      }
    }
    for (const auto& attribute : oneof_conditions) {
      if (!kSupportedAttributes.at(restriction.first).contains(attribute)) {
        if (errors) {
          errors->AddError(policy_name,
                           IDS_POLICY_DATA_CONTROLS_UNSUPPORTED_CONDITION,
                           std::string(attribute),
                           RestrictionToString(restriction.first), error_path);
        }
        valid = false;
      }
    }
  }

  return valid;
}

// static
bool Rule::AddUnsupportedRestrictionErrors(
    const char* policy_name,
    const base::flat_map<Rule::Restriction, Rule::Level>& restrictions,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  static const base::flat_map<Rule::Restriction, std::set<Rule::Level>>
      kSupportedRestrictions = {
          {Restriction::kClipboard,
           {Level::kNotSet, Level::kReport, Level::kWarn, Level::kBlock}},
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
          {Restriction::kScreenshot, {Level::kNotSet, Level::kBlock}},
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
      };

  bool valid = true;
  for (const auto& restriction : restrictions) {
    if (!kSupportedRestrictions.contains(restriction.first)) {
      if (errors) {
        errors->AddError(policy_name,
                         IDS_POLICY_DATA_CONTROLS_UNSUPPORTED_RESTRICTION,
                         RestrictionToString(restriction.first), error_path);
      }
      valid = false;
      continue;
    }
    if (!kSupportedRestrictions.at(restriction.first)
             .contains(restriction.second)) {
      if (errors) {
        errors->AddError(policy_name,
                         IDS_POLICY_DATA_CONTROLS_UNSUPPORTED_LEVEL,
                         RestrictionToString(restriction.first),
                         LevelToString(restriction.second), error_path);
      }
      valid = false;
    }
  }

  return valid;
}

}  // namespace data_controls
