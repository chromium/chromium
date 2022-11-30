// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_H_
#define COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_export.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/webui/web_ui_util.h"

namespace policy {

class PolicyConversionsClient;

extern const POLICY_EXPORT webui::LocalizedString
    kPolicySources[POLICY_SOURCE_COUNT];

extern const POLICY_EXPORT char kIdKey[];
extern const POLICY_EXPORT char kNameKey[];
extern const POLICY_EXPORT char kPoliciesKey[];
extern const POLICY_EXPORT char kPolicyNamesKey[];
extern const POLICY_EXPORT char kChromePoliciesId[];
extern const POLICY_EXPORT char kChromePoliciesName[];

#if !BUILDFLAG(IS_CHROMEOS)
extern const POLICY_EXPORT char kPrecedencePoliciesId[];
extern const POLICY_EXPORT char kPrecedencePoliciesName[];
#endif  // !BUILDFLAG(IS_CHROMEOS)

// A convenience class to retrieve all policies values.
class POLICY_EXPORT PolicyConversions {
 public:
  // Maps known policy names to their schema. If a policy is not present, it is
  // not known (either through policy_templates.json or through an extension's
  // managed storage schema).
  using PolicyToSchemaMap = base::flat_map<std::string, Schema>;

  // |client| provides embedder-specific policy information and must not be
  // nullptr.
  explicit PolicyConversions(std::unique_ptr<PolicyConversionsClient> client);
  PolicyConversions(const PolicyConversions&) = delete;
  PolicyConversions& operator=(const PolicyConversions&) = delete;
  virtual ~PolicyConversions();

  // Set to get policy types as human friendly string instead of enum integer.
  // Policy types includes policy source, policy scope and policy level.
  // Enabled by default.
  virtual PolicyConversions& EnableConvertTypes(bool enabled);
  // Set to get dictionary policy value as JSON string.
  // Disabled by default.
  virtual PolicyConversions& EnableConvertValues(bool enabled);
  // Set to get device local account policies on ChromeOS.
  // Disabled by default.
  virtual PolicyConversions& EnableDeviceLocalAccountPolicies(bool enabled);
  // Set to get device basic information on ChromeOS.
  // Disabled by default.
  virtual PolicyConversions& EnableDeviceInfo(bool enabled);
  // Set to enable pretty print for all JSON string.
  // Enabled by default.
  virtual PolicyConversions& EnablePrettyPrint(bool enabled);
  // Set to get all user scope policies.
  // Enabled by default.
  virtual PolicyConversions& EnableUserPolicies(bool enabled);
  // Set to drop the policies of which value is a default one set by the policy
  // provider. Disabled by default.
  virtual PolicyConversions& SetDropDefaultValues(bool enabled);

  // Returns the policy data as a JSON string;
  virtual std::string ToJSON() = 0;

 protected:
  PolicyConversionsClient* client() { return client_.get(); }

 private:
  std::unique_ptr<PolicyConversionsClient> client_;
};

class POLICY_EXPORT DictionaryPolicyConversions : public PolicyConversions {
 public:
  explicit DictionaryPolicyConversions(
      std::unique_ptr<PolicyConversionsClient> client);
  DictionaryPolicyConversions(const DictionaryPolicyConversions&) = delete;
  DictionaryPolicyConversions& operator=(const DictionaryPolicyConversions&) =
      delete;
  ~DictionaryPolicyConversions() override;

  DictionaryPolicyConversions& EnableConvertTypes(bool enabled) override;

  DictionaryPolicyConversions& EnableConvertValues(bool enabled) override;

  DictionaryPolicyConversions& EnableDeviceLocalAccountPolicies(
      bool enabled) override;

  DictionaryPolicyConversions& EnableDeviceInfo(bool enabled) override;

  DictionaryPolicyConversions& EnablePrettyPrint(bool enabled) override;

  DictionaryPolicyConversions& EnableUserPolicies(bool enabled) override;

  DictionaryPolicyConversions& SetDropDefaultValues(bool enabled) override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  base::Value::Dict GetExtensionPolicies();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  std::string ToJSON() override;

  base::Value::Dict ToValueDict();

 private:
  base::Value::Dict GetExtensionPolicies(PolicyDomain policy_domain);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::Value::Dict GetDeviceLocalAccountPolicies();
#endif
};

// PolicyConversions implementation that retrieves Chrome policies. The
// retrieved policies include precedence policies for non-ChromeOS and device
// local account policies and identity fields for ChromeOS Ash.
class POLICY_EXPORT ChromePolicyConversions : public PolicyConversions {
 public:
  explicit ChromePolicyConversions(
      std::unique_ptr<PolicyConversionsClient> client);
  ChromePolicyConversions(const ChromePolicyConversions&) = delete;
  ChromePolicyConversions& operator=(const ChromePolicyConversions&) = delete;
  ~ChromePolicyConversions() override;

  ChromePolicyConversions& EnableConvertTypes(bool enabled) override;

  ChromePolicyConversions& EnableConvertValues(bool enabled) override;

  ChromePolicyConversions& EnableDeviceLocalAccountPolicies(
      bool enabled) override;

  ChromePolicyConversions& EnableDeviceInfo(bool enabled) override;

  ChromePolicyConversions& EnablePrettyPrint(bool enabled) override;

  ChromePolicyConversions& EnableUserPolicies(bool enabled) override;

  ChromePolicyConversions& SetDropDefaultValues(bool enabled) override;

  std::string ToJSON() override;

  base::Value::Dict ToValueDict();

 private:
  base::Value::Dict GetChromePolicies();
#if !BUILDFLAG(IS_CHROMEOS)
  base::Value::Dict GetPrecedencePolicies();
#endif  // !BUILDFLAG(IS_CHROMEOS)
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_H_
