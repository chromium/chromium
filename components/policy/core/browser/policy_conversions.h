// Copyright 2013 The Chromium Authors. All rights reserved.
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
  // Set to get extension policies.
  // Enabled by default.
  // TODO(b/233209041): Remove this option when extension policies are removed
  // from ArrayPolicyConversions.
  virtual PolicyConversions& EnableExtensionPolicies(bool enabled);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Sets the updater policies.
  virtual PolicyConversions& WithUpdaterPolicies(
      std::unique_ptr<PolicyMap> policies);

  // Sets the updater policy schemas.
  virtual PolicyConversions& WithUpdaterPolicySchemas(
      PolicyToSchemaMap schemas);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Returns the policy data as a JSON string;
  virtual std::string ToJSON() = 0;

 protected:
  PolicyConversionsClient* client() { return client_.get(); }

  bool extension_policies_enabled_ = true;

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

  DictionaryPolicyConversions& EnableExtensionPolicies(bool enabled) override;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Sets the updater policies.
  DictionaryPolicyConversions& WithUpdaterPolicies(
      std::unique_ptr<PolicyMap> policies) override;

  // Sets the updater policy schemas.
  DictionaryPolicyConversions& WithUpdaterPolicySchemas(
      PolicyToSchemaMap schemas) override;
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

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

class POLICY_EXPORT ArrayPolicyConversions : public PolicyConversions {
 public:
  explicit ArrayPolicyConversions(
      std::unique_ptr<PolicyConversionsClient> client);
  ArrayPolicyConversions(const ArrayPolicyConversions&) = delete;
  ArrayPolicyConversions& operator=(const ArrayPolicyConversions&) = delete;
  ~ArrayPolicyConversions() override;

  ArrayPolicyConversions& EnableConvertTypes(bool enabled) override;

  ArrayPolicyConversions& EnableConvertValues(bool enabled) override;

  ArrayPolicyConversions& EnableDeviceLocalAccountPolicies(
      bool enabled) override;

  ArrayPolicyConversions& EnableDeviceInfo(bool enabled) override;

  ArrayPolicyConversions& EnablePrettyPrint(bool enabled) override;

  ArrayPolicyConversions& EnableUserPolicies(bool enabled) override;

  ArrayPolicyConversions& SetDropDefaultValues(bool enabled) override;

  ArrayPolicyConversions& EnableExtensionPolicies(bool enabled) override;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Sets the updater policies.
  ArrayPolicyConversions& WithUpdaterPolicies(
      std::unique_ptr<PolicyMap> policies) override;

  // Sets the updater policy schemas.
  ArrayPolicyConversions& WithUpdaterPolicySchemas(
      PolicyToSchemaMap schemas) override;
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  std::string ToJSON() override;

  base::Value::List ToValueList();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Additional Chrome policies that need to be displayed, though not available
  // through policy service.
  void WithAdditionalChromePolicies(base::Value::Dict policies);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
  base::Value::Dict GetChromePolicies();
  base::Value::Dict GetPrecedencePolicies();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Returns extension policies in a list.
  base::Value::List GetExtensionPolicies();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::Value::Dict GetUpdaterPolicies();
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  base::Value::Dict additional_chrome_policies_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_H_
