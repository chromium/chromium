// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_CLIENT_H_
#define COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_CLIENT_H_

#include <optional>
#include <set>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_export.h"

namespace policy {

class ConfigurationPolicyHandlerList;
class PolicyErrorMap;
class PolicyService;
class Schema;
class SchemaMap;
class SchemaRegistry;

using PoliciesSet = std::set<std::string>;

// PolicyConversionsClient supplies embedder-specific information that is needed
// by the PolicyConversions class.  It also provides common utilities and
// helpers needed to compute and format policy information.  Embedders must
// subclass PolicyConversionsClient and provide an instance to
// PolicyConversions.
class POLICY_EXPORT PolicyConversionsClient {
 public:
  PolicyConversionsClient();
  virtual ~PolicyConversionsClient();

  // Set to get policy types as human friendly string instead of enum integer.
  // Policy types includes policy source, policy scope and policy level.
  // Enabled by default.
  void EnableConvertTypes(bool enabled);
  // Set to get dictionary policy value as JSON string.
  // Disabled by default.
  void EnableConvertValues(bool enabled);
  // Set to get device local account policies on ChromeOS.
  // Disabled by default.
  void EnableDeviceLocalAccountPolicies(bool enabled);
  // Set to get device basic information on ChromeOS.
  // Disabled by default.
  void EnableDeviceInfo(bool enabled);
  // Set to enable pretty print for all JSON string.
  // Enabled by default.
  void EnablePrettyPrint(bool enabled);
  // Set to get all user scope policies.
  // Enabled by default.
  void EnableUserPolicies(bool enabled);
  // Set to drop the policies of which value is a default one set by the policy
  // provider. Disabled by default.
  void SetDropDefaultValues(bool enabled);
  // Set to show policy values set by machine scope sources including CBCM or
  // GPO. When set to false, policies are still included, but values and errors
  // will be hidden. Used when caller don't have permission to view those
  // values. Enabled by default.
  void EnableShowMachineValues(bool enabled);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::Value::Dict ConvertUpdaterPolicies(
      PolicyMap updater_policies,
      std::optional<PolicyConversions::PolicyToSchemaMap>
          updater_policy_schemas);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Converts the given |value| to JSON, respecting the configuration
  // preferences that were set on this client.
  std::string ConvertValueToJSON(const base::Value& value) const;

  // Returns policies for Chrome browser. Must only be called if
  // |HasUserPolicies()| returns true.
  base::Value::Dict GetChromePolicies();

  // Returns precedence-related policies for Chrome browser. Must only be called
  // if |HasUserPolicies()| returns true.
  base::Value::Dict GetPrecedencePolicies();

  // Returns an array containing the ordered precedence strings.
  base::Value::List GetPrecedenceOrder();

  // Returns true if this client is able to return information on user
  // policies.
  virtual bool HasUserPolicies() const = 0;

  // Returns policies for Chrome extensions in a list of base::Value::Dict.
  virtual base::Value::List GetExtensionPolicies(
      PolicyDomain policy_domain) = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns policies for ChromeOS device.
  virtual base::Value::List GetDeviceLocalAccountPolicies() = 0;
  // Returns device specific information if this device is enterprise managed.
  virtual base::Value::Dict GetIdentityFields() = 0;
#endif

  // Returns the embedder's PolicyService.
  virtual PolicyService* GetPolicyService() const = 0;

  // Returns the embedder's SchemaRegistry.
  virtual SchemaRegistry* GetPolicySchemaRegistry() const = 0;

  // Returns the embedder's ConfigurationPolicyHandlerList.
  virtual const ConfigurationPolicyHandlerList* GetHandlerList() const = 0;

  // Returns whether this client was configured to get device local account
  // policies on ChromeOS.
  bool GetDeviceLocalAccountPoliciesEnabled() const;
  // Returns whether this client was configured to get device basic information
  // on ChromeOS.
  bool GetDeviceInfoEnabled() const;
  // Returns whether this client was configured to get all user scope policies.
  bool GetUserPoliciesEnabled() const;

 protected:
  // Returns a copy of |value|. If necessary (which is specified by
  // |convert_values_enabled_|), converts some values to a representation that
  // i18n_template.js will display.
  base::Value CopyAndMaybeConvert(const base::Value& value,
                                  const std::optional<Schema>& schema,
                                  PolicyScope scope) const;

  // Creates a description of the policy |policy_name| using |policy| and the
  // optional errors in |errors| to determine the status of each policy.
  // |known_policy_schemas| contains |Schema|s for known policies in the same
  // policy namespace of |map|. |deprecated_policies| holds deprecated policies.
  // |future_policies| holds unreleased policies. A policy without an entry in
  // |known_policy_schemas| is an unknown policy.
  base::Value::Dict GetPolicyValue(
      const std::string& policy_name,
      const PolicyMap::Entry& policy,
      const PoliciesSet& deprecated_policies,
      const PoliciesSet& future_policies,
      PolicyErrorMap* errors,
      const std::optional<PolicyConversions::PolicyToSchemaMap>&
          known_policy_schemas) const;

  // Returns a description of each policy in |map| as Value, using the
  // optional errors in |errors| to determine the status of each policy.
  // |known_policy_schemas| contains |Schema|s for known policies in the same
  // policy namespace of |map|. |deprecated_policies| holds deprecated policies.
  // |future_policies| holds unreleased policies. A policy in |map| but without
  // an entry |known_policy_schemas| is an unknown policy.
  base::Value::Dict GetPolicyValues(
      const PolicyMap& map,
      PolicyErrorMap* errors,
      const PoliciesSet& deprecated_policies,
      const PoliciesSet& future_policies,
      const std::optional<PolicyConversions::PolicyToSchemaMap>&
          known_policy_schemas) const;

  // Returns the Schema for |policy_name| if that policy is known. If the policy
  // is unknown, returns |std::nullopt|.
  std::optional<Schema> GetKnownPolicySchema(
      const std::optional<PolicyConversions::PolicyToSchemaMap>&
          known_policy_schemas,
      const std::string& policy_name) const;

  std::optional<PolicyConversions::PolicyToSchemaMap> GetKnownPolicies(
      const scoped_refptr<SchemaMap> schema_map,
      const PolicyNamespace& policy_namespace) const;

 private:
  friend class PolicyConversionsClientTest;

  // Returns the policy scope to be used for UI. The |policy_scope| from the
  // input is the generic scope: device or user policy. But in Lacros case we
  // need to filter the user policies based on per_profile flag.
  std::string GetPolicyScope(const std::string& policy_name,
                             const PolicyScope& policy_scope) const;

  std::u16string GetPolicyError(
      const std::string& policy_name,
      const PolicyMap::Entry& policy,
      PolicyErrorMap* errors,
      std::optional<Schema> known_policy_schema) const;

  bool convert_types_enabled_ = true;
  bool convert_values_enabled_ = false;
  bool device_local_account_policies_enabled_ = false;
  bool device_info_enabled_ = false;
  bool pretty_print_enabled_ = true;
  bool user_policies_enabled_ = true;
  bool drop_default_values_enabled_ = false;
  bool show_machine_values_ = true;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void PopulatePerProfileMap();
  std::unique_ptr<std::map<std::string, bool>> per_profile_map_;
#endif
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_CLIENT_H_
