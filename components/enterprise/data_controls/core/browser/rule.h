// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_RULE_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_RULE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/values.h"
#include "components/enterprise/data_controls/core/browser/action_context.h"
#include "components/enterprise/data_controls/core/browser/conditions/condition.h"
#include "components/policy/core/common/schema.h"

namespace policy {
class PolicyErrorMap;
}  // namespace policy

namespace data_controls {

// Constants used to parse sub-dictionaries of DLP policies that should map to
// an AttributesCondition.
inline constexpr char kRestrictionClipboard[] = "CLIPBOARD";
inline constexpr char kRestrictionScreenshot[] = "SCREENSHOT";
inline constexpr char kRestrictionPrinting[] = "PRINTING";
inline constexpr char kRestrictionPrivacyScreen[] = "PRIVACY_SCREEN";
inline constexpr char kRestrictionScreenShare[] = "SCREEN_SHARE";
inline constexpr char kRestrictionFiles[] = "FILES";

inline constexpr char kLevelAllow[] = "ALLOW";
inline constexpr char kLevelBlock[] = "BLOCK";
inline constexpr char kLevelWarn[] = "WARN";
inline constexpr char kLevelReport[] = "REPORT";

// Implementation of a Data Controls policy rule, which provides interfaces to
// evaluate its conditions to obtain verdicts and access other rule attributes.
// This class is a representation of the following JSON:
// {
//   name: string,
//   rule_id: string,
//   description: string,
//
//   sources: { See schema in attributes_condition.h }
//   destinations: { See schema in attributes_condition.h }
//
//   restrictions: [
//     {
//       class: CLIPBOARD|SCREENSHOT|PRINTING|PRIVACY_SCREEN|etc,
//       level: ALLOW|BLOCK|REPORT|WARN
//     }
//   ]
// }
class Rule {
 public:
  // A restriction that can be set by Data Controls.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // When new entries are added, EnterpriseDlpPolicyRestriction enum in
  // histograms/enums.xml should be updated.
  enum class Restriction {
    kUnknownRestriction = 0,
    kClipboard = 1,      // Restricts sharing the data via clipboard and
                         // drag-n-drop.
    kScreenshot = 2,     // Restricts taking screenshots and video captures of
                         // confidential screen content.
    kPrinting = 3,       // Restricts printing confidential screen content.
    kPrivacyScreen = 4,  // Enforces the Eprivacy screen when there's
                         // confidential content on the screen.
    kScreenShare = 5,    // Restricts screen sharing of confidential content
                         // through 3P extensions/websites.
    kFiles = 6,          // Restricts file operations, like copying, uploading
                         // or opening in an app.
    kMaxValue = kFiles
  };

  // The enforcement level of the restriction set by Data Controls.
  // Should be listed in the order of increased priority.
  // When new entries are added, EnterpriseDlpPolicyLevel enum in
  // histograms/enums.xml should be updated.
  enum class Level {
    kNotSet = 0,  // Restriction level is not set.
    kReport = 1,  // Restriction level to only report on every action.
    kWarn = 2,    // Restriction level to warn the user on every action.
    kBlock = 3,   // Restriction level to block the user on every action.
    kAllow = 4,   // Restriction level to allow (no restriction).
    kMaxValue = kAllow
  };

  // Returns nullopt if the passed JSON doesn't match the expected schema.
  static std::optional<Rule> Create(const base::Value& value);
  static std::optional<Rule> Create(const base::Value::Dict& value);

  // Helpers to help conversions when parsing JSON.
  static Restriction StringToRestriction(const std::string& restriction);
  static Level StringToLevel(const std::string& level);
  static const char* RestrictionToString(Restriction restriction);
  static const char* LevelToString(Level level);

  // Helpers used by Data Controls's policy handler to validate rules, and add
  // relevant context to `errors. It is assumed `value` has had its schema
  // validated by SchemaValidatingPolicyHandler.
  static bool ValidateRuleValue(const char* policy_name,
                                const base::Value::Dict& root_value,
                                policy::PolicyErrorPath error_path,
                                policy::PolicyErrorMap* errors);

  Rule(Rule&& other);
  ~Rule();

  // Returns the `Level` to be applied to a given action.
  Level GetLevel(Restriction restriction, const ActionContext& context) const;

  const std::string& name() const;
  const std::string& rule_id() const;
  const std::string& description() const;

 private:
  Rule(std::string name,
       std::string rule_id,
       std::string description,
       std::unique_ptr<const Condition> condition,
       base::flat_map<Restriction, Level> restrictions);

  // Helper to parse sub-fields controlling conditions and combine them into a
  // single `Condition` object. This is called on the "root" level of the
  // condition and recursively as needed.
  static std::unique_ptr<const Condition> GetCondition(
      const base::Value::Dict& value);

  // Helper to parse sub-fields controlling conditions under "sources" and/or
  // "destinations" and combine them into a single `Condition` object.
  static std::unique_ptr<const Condition> GetSourcesAndDestinationsCondition(
      const base::Value::Dict& value);

  // Helper to parse the JSON list of conditions under a "and" or "or" key.
  static std::vector<std::unique_ptr<const Condition>> GetListConditions(
      const base::Value::List& value);

  // Helper to parse the following JSON schema:
  // {
  //   class: CLIPBOARD|SCREENSHOT|PRINTING|PRIVACY_SCREEN|etc,
  //   level: ALLOW|BLOCK|REPORT|WARN
  // }
  // For compatibility, unrecognized values are ignored and valid values are
  // still included in the output.
  static base::flat_map<Restriction, Level> GetRestrictions(
      const base::Value::Dict& value);

  // Helper used to recursively validate a rule. This should only be called by
  // itself and `ValidateRuleValue`.
  static bool ValidateRuleSubValues(
      const char* policy_name,
      const base::Value::Dict& value,
      const base::flat_map<Restriction, Level>& restrictions,
      policy::PolicyErrorPath error_path,
      policy::PolicyErrorMap* errors);

  // Helper called by `ValidateRuleSubValues` to populate errors related to
  // mutually exclusive fields being used in a rule.
  static void AddMutuallyExclusiveErrors(
      const std::vector<std::string_view>& oneof_conditions,
      const std::vector<std::string_view>& anyof_conditions,
      const char* policy_name,
      policy::PolicyErrorPath error_path,
      policy::PolicyErrorMap* errors);

  // Helper called by `ValidateRuleSubValues` to check that all attributes
  // included in a rule are meaningful to the restrictions included in that
  // rule. Returns false if at least one error was added.
  static bool AddUnsupportedAttributeErrors(
      const std::vector<std::string_view>& oneof_conditions,
      const std::vector<std::string_view>& anyof_conditions,
      base::flat_map<Restriction, Level> restrictions,
      const char* policy_name,
      policy::PolicyErrorPath error_path,
      policy::PolicyErrorMap* errors);

  // Helper called by `ValidateRuleSubValues` to check that all given
  // restrictions are applicable to the rule provided in `value`. Returns false
  // if at least one error was added.
  static bool AddUnsupportedRestrictionErrors(
      const char* policy_name,
      const base::flat_map<Restriction, Level>& restrictions,
      policy::PolicyErrorPath error_path,
      policy::PolicyErrorMap* errors);

  // Metadata fields directly taken from the rule's JSON.
  const std::string name_;
  const std::string rule_id_;
  const std::string description_;

  // The conditions that should trigger for the rule to apply. This should never
  // be null.
  std::unique_ptr<const Condition> condition_;

  // The `Restriction` => `Level` mapping the rule should apply.
  const base::flat_map<Restriction, Level> restrictions_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_RULE_H_
