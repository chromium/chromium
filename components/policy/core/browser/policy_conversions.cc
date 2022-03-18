// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_conversions.h"

#include <utility>

#include "base/check.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

using base::Value;

namespace policy {

const webui::LocalizedString kPolicySources[POLICY_SOURCE_COUNT] = {
    {"sourceEnterpriseDefault", IDS_POLICY_SOURCE_ENTERPRISE_DEFAULT},
    {"commandLine", IDS_POLICY_SOURCE_COMMAND_LINE},
    {"cloud", IDS_POLICY_SOURCE_CLOUD},
    {"sourceActiveDirectory", IDS_POLICY_SOURCE_ACTIVE_DIRECTORY},
    {"sourceDeviceLocalAccountOverrideDeprecated",
     IDS_POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE},
    {"platform", IDS_POLICY_SOURCE_PLATFORM},
    {"priorityCloud", IDS_POLICY_SOURCE_CLOUD},
    {"merged", IDS_POLICY_SOURCE_MERGED},
    {"cloud_from_ash", IDS_POLICY_SOURCE_CLOUD_FROM_ASH},
    {"restrictedManagedGuestSessionOverride",
     IDS_POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE},
};

PolicyConversions::PolicyConversions(
    std::unique_ptr<PolicyConversionsClient> client)
    : client_(std::move(client)) {
  DCHECK(client_.get());
}

PolicyConversions::~PolicyConversions() = default;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
PolicyConversions& PolicyConversions::WithUpdaterPolicies(
    std::unique_ptr<PolicyMap> policies) {
  client()->SetUpdaterPolicies(std::move(policies));
  return *this;
}
PolicyConversions& PolicyConversions::WithUpdaterPolicySchemas(
    PolicyToSchemaMap schemas) {
  client()->SetUpdaterPolicySchemas(std::move(schemas));
  return *this;
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

PolicyConversions& PolicyConversions::EnableConvertTypes(bool enabled) {
  client_->EnableConvertTypes(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::EnableConvertValues(bool enabled) {
  client_->EnableConvertValues(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::EnableDeviceLocalAccountPolicies(
    bool enabled) {
  client_->EnableDeviceLocalAccountPolicies(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::EnableDeviceInfo(bool enabled) {
  client_->EnableDeviceInfo(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::EnablePrettyPrint(bool enabled) {
  client_->EnablePrettyPrint(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::EnableUserPolicies(bool enabled) {
  client_->EnableUserPolicies(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::SetDropDefaultValues(bool enabled) {
  client_->SetDropDefaultValues(enabled);
  return *this;
}

std::string PolicyConversions::ToJSON() {
  return client_->ConvertValueToJSON(ToValue());
}

/**
 * DictionaryPolicyConversions
 */

DictionaryPolicyConversions::DictionaryPolicyConversions(
    std::unique_ptr<PolicyConversionsClient> client)
    : PolicyConversions(std::move(client)) {}
DictionaryPolicyConversions::~DictionaryPolicyConversions() = default;

Value DictionaryPolicyConversions::ToValue() {
  Value all_policies(Value::Type::DICTIONARY);

  if (client()->HasUserPolicies()) {
    all_policies.SetKey("chromePolicies", client()->GetChromePolicies());

#if BUILDFLAG(ENABLE_EXTENSIONS)
    all_policies.SetKey("extensionPolicies",
                        GetExtensionPolicies(POLICY_DOMAIN_EXTENSIONS));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  }

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (client()->HasUpdaterPolicies())
    all_policies.SetKey("updaterPolicies", client()->GetUpdaterPolicies());
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)
  all_policies.SetKey("loginScreenExtensionPolicies",
                      GetExtensionPolicies(POLICY_DOMAIN_SIGNIN_EXTENSIONS));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  all_policies.SetKey("deviceLocalAccountPolicies",
                      GetDeviceLocalAccountPolicies());
  Value identity_fields = client()->GetIdentityFields();
  if (!identity_fields.is_none())
    all_policies.MergeDictionary(&identity_fields);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return all_policies;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
Value DictionaryPolicyConversions::GetDeviceLocalAccountPolicies() {
  Value policies = client()->GetDeviceLocalAccountPolicies();
  Value device_values(Value::Type::DICTIONARY);
  for (auto&& policy : policies.GetListDeprecated()) {
    device_values.SetKey(policy.FindKey("id")->GetString(),
                         std::move(*policy.FindKey("policies")));
  }
  return device_values;
}
#endif

Value DictionaryPolicyConversions::GetExtensionPolicies(
    PolicyDomain policy_domain) {
  Value policies = client()->GetExtensionPolicies(policy_domain);
  Value extension_values(Value::Type::DICTIONARY);
  for (auto&& policy : policies.GetListDeprecated()) {
    extension_values.SetKey(policy.FindKey("id")->GetString(),
                            std::move(*policy.FindKey("policies")));
  }
  return extension_values;
}

/**
 * ArrayPolicyConversions
 */

ArrayPolicyConversions::ArrayPolicyConversions(
    std::unique_ptr<PolicyConversionsClient> client)
    : PolicyConversions(std::move(client)) {}
ArrayPolicyConversions::~ArrayPolicyConversions() = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ArrayPolicyConversions::WithAdditionalChromePolicies(Value&& policies) {
  additional_chrome_policies_ = std::move(policies);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

Value ArrayPolicyConversions::ToValue() {
  Value all_policies(Value::Type::LIST);

  if (client()->HasUserPolicies()) {
    all_policies.Append(GetChromePolicies());

#if !BUILDFLAG(IS_CHROMEOS)
    // Precedence policies do not apply to Chrome OS, so the Policy Precedence
    // table is not shown in chrome://policy.
    all_policies.Append(GetPrecedencePolicies());
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (client()->HasUpdaterPolicies())
      all_policies.Append(GetUpdaterPolicies());
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
    for (auto& policy : client()
                            ->GetExtensionPolicies(POLICY_DOMAIN_EXTENSIONS)
                            .TakeListDeprecated()) {
      all_policies.Append(std::move(policy));
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  }

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)
  for (auto& policy :
       client()
           ->GetExtensionPolicies(POLICY_DOMAIN_SIGNIN_EXTENSIONS)
           .TakeListDeprecated()) {
    all_policies.Append(std::move(policy));
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  for (auto& device_policy :
       client()->GetDeviceLocalAccountPolicies().TakeListDeprecated())
    all_policies.Append(std::move(device_policy));

  Value identity_fields = client()->GetIdentityFields();
  if (!identity_fields.is_none())
    all_policies.Append(std::move(identity_fields));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return all_policies;
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
Value ArrayPolicyConversions::GetUpdaterPolicies() {
  Value chrome_policies_data(Value::Type::DICTIONARY);
  chrome_policies_data.SetKey("name", Value("Google Update Policies"));
  chrome_policies_data.SetKey("id", Value("updater"));
  chrome_policies_data.SetKey("policies", client()->GetUpdaterPolicies());
  return chrome_policies_data;
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

Value ArrayPolicyConversions::GetChromePolicies() {
  Value chrome_policies_data(Value::Type::DICTIONARY);
  chrome_policies_data.SetKey("id", Value("chrome"));
  chrome_policies_data.SetKey("name", Value("Chrome Policies"));
  Value chrome_policies = client()->GetChromePolicies();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (additional_chrome_policies_ != base::Value())
    chrome_policies.MergeDictionary(&additional_chrome_policies_);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  chrome_policies_data.SetKey("policies", std::move(chrome_policies));
  return chrome_policies_data;
}

Value ArrayPolicyConversions::GetPrecedencePolicies() {
  Value precedence_policies_data(Value::Type::DICTIONARY);
  precedence_policies_data.SetKey("id", Value("precedence"));
  precedence_policies_data.SetKey("name", Value("Policy Precedence"));
  precedence_policies_data.SetKey("policies",
                                  client()->GetPrecedencePolicies());
  precedence_policies_data.SetKey("precedenceOrder",
                                  client()->GetPrecedenceOrder());
  return precedence_policies_data;
}

}  // namespace policy
