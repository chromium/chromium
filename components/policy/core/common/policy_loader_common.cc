// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_common.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

// Duplicate the extension constants in order to avoid extension dependency.
// However, those values below must be synced with files in extension folders.
// In long term, we can refactor the code and create an interface for sensitive
// policy filtering so that each policy component users can have their own
// implementation. And the Chrome one can be moved to c/b/policy.
// From extensions/common/extension_urls.cc
const char kChromeWebstoreUpdateURL[] =
    "https://clients2.google.com/service/update2/crx";
const char16_t kChromeWebstoreUpdateURL16[] =
    u"https://clients2.google.com/service/update2/crx";

// From chrome/browser/extensions/extension_management_constants.cc
const char kWildcard[] = "*";
const char kInstallationMode[] = "installation_mode";
const char kForceInstalled[] = "force_installed";
const char kNormalInstalled[] = "normal_installed";
const char kUpdateUrl[] = "update_url";

// String to be prepended to each blocked entry.
const char kBlockedExtensionPrefix[] = "[BLOCKED]";

// List of policies that are considered only if the user is part of a AD domain
// on Windows or managed on the Mac. Please document any new additions in
// policy_templates.json!
// Please keep the list in alphabetical order!
const char* kSensitivePolicies[] = {
    key::kAutoOpenFileTypes,
    key::kCommandLineFlagSecurityWarningsEnabled,
    key::kDefaultSearchProviderEnabled,
    key::kFirstPartySetsOverrides,
    key::kHomepageIsNewTabPage,
    key::kHomepageLocation,
    key::kMetricsReportingEnabled,
    key::kNewTabPageLocation,
    key::kPasswordProtectionChangePasswordURL,
    key::kPasswordProtectionLoginURLs,
    key::kRestoreOnStartup,
    key::kRestoreOnStartupURLs,
    key::kSafeBrowsingEnabled,
    key::kSafeBrowsingAllowlistDomains,
#if BUILDFLAG(IS_WIN)
    key::kChromeCleanupEnabled,
    key::kChromeCleanupReportingEnabled,
    key::kSafeBrowsingForTrustedSourcesEnabled,
#endif
};

void RecordInvalidPolicies(const std::string& policy_name) {
  const PolicyDetails* details = GetChromePolicyDetails(policy_name);
  base::UmaHistogramSparse("EnterpriseCheck.InvalidPolicies", details->id);
}

// Marks the sensitive ExtensionInstallForceList policy entries, returns true if
// there is any sensitive entries in the policy.
bool FilterSensitiveExtensionsInstallForcelist(PolicyMap::Entry* map_entry) {
  bool has_invalid_policies = false;
  if (!map_entry)
    return false;

  base::Value* policy_list_value = map_entry->value(base::Value::Type::LIST);
  if (!policy_list_value)
    return false;

  // Using index for loop to update the list in place.
  for (size_t i = 0; i < policy_list_value->GetList().size(); i++) {
    const auto& list_entry = policy_list_value->GetList()[i];
    if (!list_entry.is_string())
      continue;

    const std::string& entry = list_entry.GetString();
    size_t pos = entry.find(';');
    if (pos == std::string::npos)
      continue;

    // Only allow custom update urls in enterprise environments.
    if (!base::EqualsCaseInsensitiveASCII(entry.substr(pos + 1),
                                          kChromeWebstoreUpdateURL)) {
      policy_list_value->GetList()[i] =
          base::Value(kBlockedExtensionPrefix + entry);
      has_invalid_policies = true;
    }
  }

  if (has_invalid_policies) {
    map_entry->AddMessage(PolicyMap::MessageType::kWarning,
                          IDS_POLICY_OFF_CWS_URL_ERROR,
                          {kChromeWebstoreUpdateURL16});
    RecordInvalidPolicies(key::kExtensionInstallForcelist);
  }

  return has_invalid_policies;
}

// Marks the sensitive ExtensionSettings policy entries, returns the number of
// sensitive entries in the policy.
bool FilterSensitiveExtensionSettings(PolicyMap::Entry* map_entry) {
  if (!map_entry)
    return false;
  base::Value* policy_dict_value = map_entry->value(base::Value::Type::DICT);
  if (!policy_dict_value) {
    return false;
  }

  // Note that we only search for sensitive entries, all other validations will
  // be handled by ExtensionSettingsPolicyHandler.
  std::vector<std::string> filtered_extensions;
  for (auto entry : policy_dict_value->DictItems()) {
    if (entry.first == kWildcard)
      continue;
    if (!entry.second.is_dict())
      continue;
    std::string* installation_mode =
        entry.second.FindStringKey(kInstallationMode);
    if (!installation_mode || (*installation_mode != kForceInstalled &&
                               *installation_mode != kNormalInstalled)) {
      continue;
    }
    std::string* update_url = entry.second.FindStringKey(kUpdateUrl);
    if (!update_url || base::EqualsCaseInsensitiveASCII(
                           *update_url, kChromeWebstoreUpdateURL)) {
      continue;
    }

    filtered_extensions.push_back(entry.first);
  }

  // Marking the blocked extension by adding the "[BLOCKED]" prefix. This is an
  // invalid extension id and will be removed by PolicyHandler later.
  if (!filtered_extensions.empty()) {
    for (const auto& extension : filtered_extensions) {
      auto setting = policy_dict_value->ExtractKey(extension);
      if (!setting)
        continue;
      policy_dict_value->SetKey(kBlockedExtensionPrefix + extension,
                                std::move(setting.value()));
    }
    map_entry->AddMessage(PolicyMap::MessageType::kWarning,
                          IDS_POLICY_OFF_CWS_URL_ERROR,
                          {kChromeWebstoreUpdateURL16});
    RecordInvalidPolicies(key::kExtensionSettings);
  }

  return !filtered_extensions.empty();
}

}  // namespace

void FilterSensitivePolicies(PolicyMap* policy) {
  int invalid_policies = 0;
  if (FilterSensitiveExtensionsInstallForcelist(
          policy->GetMutable(key::kExtensionInstallForcelist))) {
    invalid_policies++;
  }
  if (FilterSensitiveExtensionSettings(
          policy->GetMutable(key::kExtensionSettings))) {
    invalid_policies++;
  }
  for (const char* sensitive_policy : kSensitivePolicies) {
    if (policy->Get(sensitive_policy)) {
      policy->GetMutable(sensitive_policy)->SetBlocked();
      invalid_policies++;
      RecordInvalidPolicies(sensitive_policy);
    }
  }

  UMA_HISTOGRAM_COUNTS_1M("EnterpriseCheck.InvalidPoliciesDetected",
                          invalid_policies);
}  // namespace policy

}  // namespace policy
