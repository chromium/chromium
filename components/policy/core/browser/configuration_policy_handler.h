// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/enum_set.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_export.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
struct PolicyHandlerParameters;
class PolicyMap;

extern POLICY_EXPORT const size_t kMaxUrlFiltersPerPolicy;

// Maps a policy type to a preference path, and to the expected value type.
struct POLICY_EXPORT PolicyToPreferenceMapEntry {
  const char* const policy_name;
  const char* const preference_path;
  const base::Value::Type value_type;
};

// An abstract super class that subclasses should implement to map policies to
// their corresponding preferences, and to check whether the policies are valid.
class POLICY_EXPORT ConfigurationPolicyHandler {
 public:
  ConfigurationPolicyHandler();
  ConfigurationPolicyHandler(const ConfigurationPolicyHandler&) = delete;
  ConfigurationPolicyHandler& operator=(const ConfigurationPolicyHandler&) =
      delete;
  virtual ~ConfigurationPolicyHandler();

  // Returns whether the policy settings handled by this
  // ConfigurationPolicyHandler can be applied.  Fills |errors| with error
  // messages or warnings.  |errors| may contain error messages even when
  // |CheckPolicySettings()| returns true.
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) = 0;

  // Processes the policies handled by this ConfigurationPolicyHandler and sets
  // the appropriate preferences in |prefs|.
  //
  // This method should only be called after |CheckPolicySettings()| returns
  // true.
  virtual void ApplyPolicySettingsWithParameters(
      const PolicyMap& policies,
      const PolicyHandlerParameters& parameters,
      PrefValueMap* prefs);

  // Modifies the values of some of the policies in |policies| so that they
  // are more suitable to display to the user. This can be used to remove
  // sensitive values such as passwords, or to pretty-print values.
  virtual void PrepareForDisplaying(PolicyMap* policies) const;

 protected:
  // This is a convenience version of ApplyPolicySettingsWithParameters()
  // for derived classes that leaves out the |parameters|. Anyone extending
  // ConfigurationPolicyHandler should implement either
  // ApplyPolicySettingsWithParameters directly and implement
  // ApplyPolicySettings with a NOTREACHED or implement only
  // ApplyPolicySettings.
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) = 0;
};

// Abstract class derived from ConfigurationPolicyHandler that should be
// subclassed to handle policies that have a name.
class POLICY_EXPORT NamedPolicyHandler : public ConfigurationPolicyHandler {
 public:
  // TODO: migrate named policy handlers from char* to std::string_view
  explicit NamedPolicyHandler(const char* policy_name);
  ~NamedPolicyHandler() override;
  NamedPolicyHandler(const NamedPolicyHandler&) = delete;
  NamedPolicyHandler& operator=(const NamedPolicyHandler&) = delete;

  const char* policy_name() const;

 private:
  // The name of the policy.
  const char* policy_name_;
};

// Abstract class derived from ConfigurationPolicyHandler that should be
// subclassed to handle a single policy (not a combination of policies).
class POLICY_EXPORT TypeCheckingPolicyHandler : public NamedPolicyHandler {
 public:
  TypeCheckingPolicyHandler(const char* policy_name,
                            base::Value::Type value_type);
  TypeCheckingPolicyHandler(const TypeCheckingPolicyHandler&) = delete;
  TypeCheckingPolicyHandler& operator=(const TypeCheckingPolicyHandler&) =
      delete;
  ~TypeCheckingPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  static bool CheckPolicySettings(const char* policy,
                                  base::Value::Type value_type,
                                  const PolicyMap::Entry* entry,
                                  PolicyErrorMap* errors);

 protected:
  // Runs policy checks and returns the policy value if successful.
  bool CheckAndGetValue(const PolicyMap& policies,
                        PolicyErrorMap* errors,
                        const base::Value** value);

  static bool CheckAndGetValue(const char* policy,
                               base::Value::Type value_type,
                               const PolicyMap::Entry* entry,
                               PolicyErrorMap* errors,
                               const base::Value** value);

 private:
  // The type the value of the policy should have.
  base::Value::Type value_type_;
};

// Policy handler that makes sure the policy value is a list and filters out any
// list entries that are not of type |list_entry_type|. Derived methods may
// apply additional filters on list entries and transform the filtered list.
class POLICY_EXPORT ListPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ListPolicyHandler(const char* policy_name, base::Value::Type list_entry_type);
  ListPolicyHandler(const ListPolicyHandler&) = delete;
  ListPolicyHandler& operator=(const ListPolicyHandler&) = delete;
  ~ListPolicyHandler() override;

  // TypeCheckingPolicyHandler methods:
  // Marked as final since overriding them could bypass filtering. Override
  // CheckListEntry() and ApplyList() instead.
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) final;

  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) final;

 protected:
  // Override this method to apply a filter for each |value| in the list.
  // |value| is guaranteed to be of type |list_entry_type_| at this point.
  // Returning false removes the value from |filtered_list| passed into
  // ApplyList(). By default, any value of type |list_entry_type_| is accepted.
  virtual bool CheckListEntry(const base::Value& value);

  // Implement this method to apply the |filtered_list| of values of type
  // |list_entry_type_| as returned from CheckAndGetList() to |prefs|.
  virtual void ApplyList(base::Value::List filtered_list,
                         PrefValueMap* prefs) = 0;

 private:
  // Checks whether the policy value is indeed a list, filters out all entries
  // that are not of type |list_entry_type_| or where CheckListEntry() returns
  // false, and appends to |filtered_list| if present. If the value is missing,
  // |filtered_list| is cleared. Sets errors for filtered list entries if
  // |errors| is not nullptr.
  bool CheckAndGetList(const policy::PolicyMap& policies,
                       policy::PolicyErrorMap* errors,
                       std::optional<base::Value::List>& filtered_list);

  // Expected value type for list entries. All other types are filtered out.
  base::Value::Type list_entry_type_;
};

// Abstract class derived from TypeCheckingPolicyHandler that ensures an int
// policy's value lies in an allowed range. Either clamps or rejects values
// outside the range.
class POLICY_EXPORT IntRangePolicyHandlerBase
    : public TypeCheckingPolicyHandler {
 public:
  IntRangePolicyHandlerBase(const char* policy_name,
                            int min,
                            int max,
                            bool clamp);
  IntRangePolicyHandlerBase(const IntRangePolicyHandlerBase&) = delete;
  IntRangePolicyHandlerBase& operator=(const IntRangePolicyHandlerBase&) =
      delete;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

 protected:
  ~IntRangePolicyHandlerBase() override;

  // Ensures that the value is in the allowed range. Returns false if the value
  // cannot be parsed or lies outside the allowed range and clamping is
  // disabled.
  bool EnsureInRange(const base::Value* input,
                     int* output,
                     PolicyErrorMap* errors);

 private:
  // The minimum value allowed.
  int min_;

  // The maximum value allowed.
  int max_;

  // Whether to clamp values lying outside the allowed range instead of
  // rejecting them.
  bool clamp_;
};

// ConfigurationPolicyHandler for policies that map directly to a preference.
class POLICY_EXPORT SimplePolicyHandler : public TypeCheckingPolicyHandler {
 public:
  SimplePolicyHandler(const char* policy_name,
                      const char* pref_path,
                      base::Value::Type value_type);
  SimplePolicyHandler(const SimplePolicyHandler&) = delete;
  SimplePolicyHandler& operator=(const SimplePolicyHandler&) = delete;
  ~SimplePolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // The DictionaryValue path of the preference the policy maps to.
  const char* pref_path_;
};

// ConfigurationPolicyHandler for policies that rely on another policy to take
// effect.
class POLICY_EXPORT PolicyWithDependencyHandler : public NamedPolicyHandler {
 public:
  enum class DependencyRequirement {
    kPolicySet,
    kPolicySetWithValue,
    kPolicyUnsetOrSetWithvalue
  };

  PolicyWithDependencyHandler(const char* required_policy_name,
                              DependencyRequirement dependency_requirement,
                              base::Value expected_dependency_value,
                              std::unique_ptr<NamedPolicyHandler> handler);
  PolicyWithDependencyHandler(const PolicyWithDependencyHandler&) = delete;
  PolicyWithDependencyHandler& operator=(const PolicyWithDependencyHandler&) =
      delete;
  ~PolicyWithDependencyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  void ApplyPolicySettingsWithParameters(
      const policy::PolicyMap& policies,
      const policy::PolicyHandlerParameters& parameters,
      PrefValueMap* prefs) override;

 protected:
  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const char* required_policy_name_;
  DependencyRequirement dependency_requirement_;
  base::Value expected_dependency_value_;
  std::unique_ptr<NamedPolicyHandler> handler_;
};

// Base class that encapsulates logic for mapping from a string enum list
// to a separate matching type value.
class POLICY_EXPORT StringMappingListPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  // Data structure representing the map between policy strings and
  // matching pref values.
  class POLICY_EXPORT MappingEntry {
   public:
    MappingEntry(const char* policy_value, std::unique_ptr<base::Value> map);
    ~MappingEntry();

    const char* enum_value;
    std::unique_ptr<base::Value> mapped_value;
  };

  // Callback that generates the map for this instance.
  using GenerateMapCallback = base::RepeatingCallback<void(
      std::vector<std::unique_ptr<MappingEntry>>*)>;

  StringMappingListPolicyHandler(const char* policy_name,
                                 const char* pref_path,
                                 const GenerateMapCallback& map_generator);
  StringMappingListPolicyHandler(const StringMappingListPolicyHandler&) =
      delete;
  StringMappingListPolicyHandler& operator=(
      const StringMappingListPolicyHandler&) = delete;
  ~StringMappingListPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Attempts to convert the list in |input| to |output| according to the table,
  // returns false on errors.
  bool Convert(const base::Value* input,
               base::Value::List* output,
               PolicyErrorMap* errors);

  // Helper method that converts from a policy value string to the associated
  // pref value.
  std::unique_ptr<base::Value> Map(const std::string& entry_value);

  // Name of the pref to write.
  const char* pref_path_;

  // The callback invoked to generate the map for this instance.
  GenerateMapCallback map_getter_;

  // Map of string policy values to local pref values. This is generated lazily
  // so the generation does not have to happen if no policy is present.
  std::vector<std::unique_ptr<MappingEntry>> map_;
};

// A policy handler implementation that ensures an int policy's value lies in an
// allowed range.
class POLICY_EXPORT IntRangePolicyHandler : public IntRangePolicyHandlerBase {
 public:
  IntRangePolicyHandler(const char* policy_name,
                        const char* pref_path,
                        int min,
                        int max,
                        bool clamp);
  IntRangePolicyHandler(const IntRangePolicyHandler&) = delete;
  IntRangePolicyHandler& operator=(const IntRangePolicyHandler&) = delete;
  ~IntRangePolicyHandler() override;

  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Name of the pref to write.
  const char* pref_path_;
};

// A policy handler implementation that maps an int percentage value to a
// double.
class POLICY_EXPORT IntPercentageToDoublePolicyHandler
    : public IntRangePolicyHandlerBase {
 public:
  IntPercentageToDoublePolicyHandler(const char* policy_name,
                                     const char* pref_path,
                                     int min,
                                     int max,
                                     bool clamp);
  IntPercentageToDoublePolicyHandler(
      const IntPercentageToDoublePolicyHandler&) = delete;
  IntPercentageToDoublePolicyHandler& operator=(
      const IntPercentageToDoublePolicyHandler&) = delete;
  ~IntPercentageToDoublePolicyHandler() override;

  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Name of the pref to write.
  const char* pref_path_;
};

// Like TypeCheckingPolicyHandler, but validates against a schema instead of a
// single type. |schema| is the schema used for this policy, and |strategy| is
// the strategy used for schema validation errors.
class POLICY_EXPORT SchemaValidatingPolicyHandler : public NamedPolicyHandler {
 public:
  SchemaValidatingPolicyHandler(const char* policy_name,
                                Schema schema,
                                SchemaOnErrorStrategy strategy);
  SchemaValidatingPolicyHandler(const SchemaValidatingPolicyHandler&) = delete;
  SchemaValidatingPolicyHandler& operator=(
      const SchemaValidatingPolicyHandler&) = delete;
  ~SchemaValidatingPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

 protected:
  // Runs policy checks and returns the policy value if successful.
  bool CheckAndGetValue(const PolicyMap& policies,
                        PolicyErrorMap* errors,
                        std::unique_ptr<base::Value>* output);

 private:
  const Schema schema_;
  const SchemaOnErrorStrategy strategy_;
};

// Maps policy to pref like SimplePolicyHandler while ensuring that the value
// set matches the schema. |schema| is the schema used for policies, and
// |strategy| is the strategy used for schema validation errors.
// The |recommended_permission| and |mandatory_permission| flags indicate the
// levels at which the policy can be set. A value set at an unsupported level
// will be ignored.
class POLICY_EXPORT SimpleSchemaValidatingPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  enum MandatoryPermission { MANDATORY_ALLOWED, MANDATORY_PROHIBITED };
  enum RecommendedPermission { RECOMMENDED_ALLOWED, RECOMMENDED_PROHIBITED };

  SimpleSchemaValidatingPolicyHandler(
      const char* policy_name,
      const char* pref_path,
      Schema schema,
      SchemaOnErrorStrategy strategy,
      RecommendedPermission recommended_permission,
      MandatoryPermission mandatory_permission);
  SimpleSchemaValidatingPolicyHandler(
      const SimpleSchemaValidatingPolicyHandler&) = delete;
  SimpleSchemaValidatingPolicyHandler& operator=(
      const SimpleSchemaValidatingPolicyHandler&) = delete;
  ~SimpleSchemaValidatingPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const char* pref_path_;
  const bool allow_recommended_;
  const bool allow_mandatory_;
};

// Maps policy to pref like SimplePolicyHandler. Ensures that the root value
// of the policy is of the correct type (that is, a string, or a list, depending
// on the policy). Apart from that, all policy values are accepted without
// modification, but the |PolicyErrorMap| will be updated for every error
// encountered - for instance, if the embedded JSON is unparsable or if it does
// not match the validation schema.
// NOTE: Do not store new policies using JSON strings! If your policy has a
// complex schema, store it as a dict of that schema. This has some advantages:
// - You don't have to parse JSON every time you read it from the pref store.
// - Nested dicts are simple, but nested JSON strings are complicated.
class POLICY_EXPORT SimpleJsonStringSchemaValidatingPolicyHandler
    : public NamedPolicyHandler {
 public:
  SimpleJsonStringSchemaValidatingPolicyHandler(
      const char* policy_name,
      const char* pref_path,
      Schema schema,
      SimpleSchemaValidatingPolicyHandler::RecommendedPermission
          recommended_permission,
      SimpleSchemaValidatingPolicyHandler::MandatoryPermission
          mandatory_permission);
  SimpleJsonStringSchemaValidatingPolicyHandler(
      const SimpleJsonStringSchemaValidatingPolicyHandler&) = delete;
  SimpleJsonStringSchemaValidatingPolicyHandler& operator=(
      const SimpleJsonStringSchemaValidatingPolicyHandler&) = delete;

  ~SimpleJsonStringSchemaValidatingPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Validates |root_value| as a string. Updates |errors| if it is not valid
  // JSON or if it does not match the validation schema.
  bool CheckSingleJsonString(const base::Value* root_value,
                             PolicyErrorMap* errors);

  // Validates |root_value| as a list. Updates |errors| for each item that is
  // not a string, is not valid JSON, or doesn't match the validation schema.
  bool CheckListOfJsonStrings(const base::Value* root_value,
                              PolicyErrorMap* errors);

  // Validates that the given JSON string matches the schema. |index| is used
  // only in error messages, it is the index of the given string in the list
  // if the root value is a list, and ignored otherwise. Adds any errors it
  // finds to |errors|.
  bool ValidateJsonString(const std::string& json_string,
                          PolicyErrorMap* errors,
                          int index);

  // Record to UMA that this policy failed validation due to an error in one or
  // more embedded JSON strings - either unparsable, or didn't match the schema.
  void RecordJsonError();

  // Returns true if the schema root is a list.
  bool IsListSchema() const;

  const Schema schema_;
  const char* pref_path_;
  const bool allow_recommended_;
  const bool allow_mandatory_;
};

// A policy handler to deprecate multiple legacy policies with a new one.
// This handler will completely ignore any of legacy policy values if the new
// one is set.
class POLICY_EXPORT LegacyPoliciesDeprecatingPolicyHandler
    : public ConfigurationPolicyHandler {
 public:
  LegacyPoliciesDeprecatingPolicyHandler(
      std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
          legacy_policy_handlers,
      std::unique_ptr<NamedPolicyHandler> new_policy_handler);
  LegacyPoliciesDeprecatingPolicyHandler(
      const LegacyPoliciesDeprecatingPolicyHandler&) = delete;
  LegacyPoliciesDeprecatingPolicyHandler& operator=(
      const LegacyPoliciesDeprecatingPolicyHandler&) = delete;
  ~LegacyPoliciesDeprecatingPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettingsWithParameters(
      const policy::PolicyMap& policies,
      const policy::PolicyHandlerParameters& parameters,
      PrefValueMap* prefs) override;

 protected:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
      legacy_policy_handlers_;
  std::unique_ptr<NamedPolicyHandler> new_policy_handler_;
};

// A policy handler to deprecate a single policy with a new one. It will attempt
// to use the new value if present and then try to use the legacy value instead.
class POLICY_EXPORT SimpleDeprecatingPolicyHandler
    : public ConfigurationPolicyHandler {
 public:
  SimpleDeprecatingPolicyHandler(
      std::unique_ptr<NamedPolicyHandler> legacy_policy_handler,
      std::unique_ptr<NamedPolicyHandler> new_policy_handler);
  SimpleDeprecatingPolicyHandler(const SimpleDeprecatingPolicyHandler&) =
      delete;
  SimpleDeprecatingPolicyHandler& operator=(
      const SimpleDeprecatingPolicyHandler&) = delete;
  ~SimpleDeprecatingPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  void ApplyPolicySettingsWithParameters(
      const PolicyMap& policies,
      const PolicyHandlerParameters& parameters,
      PrefValueMap* prefs) override;

 protected:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  std::unique_ptr<NamedPolicyHandler> legacy_policy_handler_;
  std::unique_ptr<NamedPolicyHandler> new_policy_handler_;
};

// A schema policy handler for complex policies that only accept cloud sources.
class POLICY_EXPORT CloudOnlyPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  CloudOnlyPolicyHandler(const char* policy_name,
                         Schema schema,
                         SchemaOnErrorStrategy strategy);
  ~CloudOnlyPolicyHandler() override;

  // Utility method for checking whether a policy is applied by a cloud-only
  // source. Useful for cloud-only policy handlers which currently don't inherit
  // from `CloudOnlyPolicyHandler`.
  static bool CheckCloudOnlyPolicySettings(const char* policy_name,
                                           const PolicyMap& policies,
                                           PolicyErrorMap* errors);

  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
};

// A schema policy handler for complex policies that only accept user scoped
// sources.
class POLICY_EXPORT CloudUserOnlyPolicyHandler : public NamedPolicyHandler {
 public:
  CloudUserOnlyPolicyHandler(
      std::unique_ptr<NamedPolicyHandler> policy_handler);
  ~CloudUserOnlyPolicyHandler() override;

  // Utility method for checking whether a policy is applied by a user-only
  // source. Useful for user-only policy handlers which currently don't inherit
  // from `CloudUserOnlyPolicyHandler`.
  static bool CheckUserOnlyPolicySettings(const char* policy_name,
                                          const PolicyMap& policies,
                                          PolicyErrorMap* errors);

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  void ApplyPolicySettingsWithParameters(
      const policy::PolicyMap& policies,
      const policy::PolicyHandlerParameters& parameters,
      PrefValueMap* prefs) override;

 protected:
  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  std::unique_ptr<NamedPolicyHandler> policy_handler_;
};

// A schema policy handler string policies expecting a URL.
class POLICY_EXPORT URLPolicyHandler : public SimplePolicyHandler {
 public:
  URLPolicyHandler(const char* policy_name, const char* pref_path);
  ~URLPolicyHandler() override;

  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_HANDLER_H_
