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
  PolicyConversions& EnableConvertTypes(bool enabled);
  // Set to get dictionary policy value as JSON string.
  // Disabled by default.
  PolicyConversions& EnableConvertValues(bool enabled);
  // Set to get device local account policies on ChromeOS.
  // Disabled by default.
  PolicyConversions& EnableDeviceLocalAccountPolicies(bool enabled);
  // Set to get device basic information on ChromeOS.
  // Disabled by default.
  PolicyConversions& EnableDeviceInfo(bool enabled);
  // Set to enable pretty print for all JSON string.
  // Enabled by default.
  PolicyConversions& EnablePrettyPrint(bool enabled);
  // Set to get all user scope policies.
  // Enabled by default.
  PolicyConversions& EnableUserPolicies(bool enabled);
  // Set to drop the policies of which value is a default one set by the policy
  // provider. Disabled by default.
  PolicyConversions& SetDropDefaultValues(bool enabled);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Sets the updater policies.
  PolicyConversions& WithUpdaterPolicies(std::unique_ptr<PolicyMap> policies);

  // Sets the updater policy schemas.
  PolicyConversions& WithUpdaterPolicySchemas(PolicyToSchemaMap schemas);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Returns the policy data as a base::Value object.
  virtual base::Value ToValue() = 0;

  // Returns the policy data as a JSON string;
  virtual std::string ToJSON();

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

  // TODO(chromium:1321529): Investigate returning base::Value::Dict.
  base::Value ToValue() override;

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

  // TODO(chromium:1321529): Investigate returning base::Value::List.
  base::Value ToValue() override;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Additional Chrome policies that need to be displayed, though not available
  // through policy service.
  void WithAdditionalChromePolicies(base::Value&& policies);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
  base::Value::Dict GetChromePolicies();
  base::Value::Dict GetPrecedencePolicies();

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::Value::Dict GetUpdaterPolicies();
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  base::Value additional_chrome_policies_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_H_
