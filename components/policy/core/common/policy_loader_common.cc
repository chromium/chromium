// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_common.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
#endif
// List of policies that are considered only if the user is part of a AD domain
// on Windows or managed on the Mac. Please document any new additions in the
// policy definition file.
// Please keep the list in alphabetical order!
const char* kSensitivePolicies[] = {
    key::kDefaultSearchProviderEnabled,
    key::kSafeBrowsingEnabled,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
    key::kAutoOpenFileTypes,
    key::kHomepageIsNewTabPage,
    key::kPasswordProtectionChangePasswordURL,
    key::kPasswordProtectionLoginURLs,
    key::kRestoreOnStartup,
    key::kRestoreOnStartupURLs,
    key::kSafeBrowsingAllowlistDomains,
    key::kSiteSearchSettings,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    key::kCommandLineFlagSecurityWarningsEnabled,
#endif
#if !BUILDFLAG(IS_IOS)
    key::kFirstPartySetsOverrides,
    key::kHomepageLocation,
#endif
#if !BUILDFLAG(IS_ANDROID)
    key::kNewTabPageLocation,
#endif
#if !BUILDFLAG(IS_CHROMEOS)
    key::kMetricsReportingEnabled,
#endif
#if BUILDFLAG(IS_WIN)
    key::kSafeBrowsingForTrustedSourcesEnabled,
#endif
};

void RecordInvalidPolicies(const std::string& policy_name) {
  const PolicyDetails* details = GetChromePolicyDetails(policy_name);
  base::UmaHistogramSparse("EnterpriseCheck.InvalidPolicies", details->id);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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

  base::Value::Dict& policy_dict = policy_dict_value->GetDict();
  // Note that we only search for sensitive entries, all other validations will
  // be handled by ExtensionSettingsPolicyHandler.
  std::vector<std::string> filtered_extensions;
  for (auto entry : policy_dict) {
    if (entry.first == kWildcard)
      continue;
    if (!entry.second.is_dict())
      continue;
    base::Value::Dict& entry_dict = entry.second.GetDict();
    std::string* installation_mode = entry_dict.FindString(kInstallationMode);
    if (!installation_mode || (*installation_mode != kForceInstalled &&
                               *installation_mode != kNormalInstalled)) {
      continue;
    }
    std::string* update_url = entry_dict.FindString(kUpdateUrl);
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
      auto setting = policy_dict.Extract(extension);
      if (!setting)
        continue;
      policy_dict.Set(kBlockedExtensionPrefix + extension,
                      std::move(setting.value()));
    }
    map_entry->AddMessage(PolicyMap::MessageType::kWarning,
                          IDS_POLICY_OFF_CWS_URL_ERROR,
                          {kChromeWebstoreUpdateURL16});

    RecordInvalidPolicies(key::kExtensionSettings);
  }
  return !filtered_extensions.empty();
}
#endif
}  // namespace

void FilterSensitivePolicies(PolicyMap* policy) {
  int invalid_policies = 0;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (FilterSensitiveExtensionsInstallForcelist(
          policy->GetMutable(key::kExtensionInstallForcelist))) {
    invalid_policies++;
  }
  if (FilterSensitiveExtensionSettings(
          policy->GetMutable(key::kExtensionSettings))) {
    invalid_policies++;
  }
#endif
  for (const char* sensitive_policy : kSensitivePolicies) {
    if (policy->Get(sensitive_policy)) {
      policy->GetMutable(sensitive_policy)->SetBlocked();
      invalid_policies++;
      RecordInvalidPolicies(sensitive_policy);
    }
  }

  UMA_HISTOGRAM_COUNTS_1M("EnterpriseCheck.InvalidPoliciesDetected",
                          invalid_policies);
}

bool IsPolicyNameSensitive(const std::string& policy_name) {
  for (const char* sensitive_policy : kSensitivePolicies) {
    if (sensitive_policy == policy_name) {
      return true;
    }
  }
  return false;
}

}  // namespace policy
