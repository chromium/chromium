// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_common.h"

#include <optional>
#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
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

void RecordInvalidPolicies(const std::string& policy_name) {
  const PolicyDetails* details = GetChromePolicyDetails(policy_name);
  base::UmaHistogramSparse("EnterpriseCheck.InvalidPolicies", details->id);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
// Marks the sensitive ExtensionInstallForceList policy entries, returns true if
// there is any sensitive entries in the policy.
bool FilterSensitiveExtensionsInstallForcelist(PolicyMap::Entry* map_entry) {
  if (!map_entry)
    return false;

  const base::Value* const_list = map_entry->value(base::Value::Type::LIST);
  if (!const_list) {
    return false;
  }

  // Using index for loop to update the list in place.
  std::optional<base::Value> filtered_list;
  for (size_t i = 0; i < const_list->GetList().size(); ++i) {
    const auto& list_entry = const_list->GetList()[i];
    if (!list_entry.is_string())
      continue;

    const std::string& entry = list_entry.GetString();
    size_t pos = entry.find(';');
    if (pos == std::string::npos)
      continue;

    // Only allow custom update urls in enterprise environments.
    if (!base::EqualsCaseInsensitiveASCII(entry.substr(pos + 1),
                                          kChromeWebstoreUpdateURL)) {
      if (!filtered_list) {
        filtered_list = const_list->Clone();
      }
      filtered_list->GetList()[i] =
          base::Value(kBlockedExtensionPrefix + entry);
    }
  }

  if (filtered_list) {
    map_entry->set_value(std::move(*filtered_list));
    map_entry->AddMessage(PolicyMap::MessageType::kWarning,
                          IDS_POLICY_OFF_CWS_URL_ERROR,
                          {kChromeWebstoreUpdateURL16});

    RecordInvalidPolicies(key::kExtensionInstallForcelist);
    return true;
  }

  return false;
}

// Marks the sensitive ExtensionSettings policy entries, returns the number of
// sensitive entries in the policy.
bool FilterSensitiveExtensionSettings(PolicyMap::Entry* map_entry) {
  if (!map_entry)
    return false;
  const base::Value* const_dict_value =
      map_entry->value(base::Value::Type::DICT);
  if (!const_dict_value) {
    return false;
  }

  // Note that we only search for sensitive entries, all other validations will
  // be handled by ExtensionSettingsPolicyHandler.
  std::vector<std::string> filtered_extensions;
  for (const auto [extension_id, entry_value] : const_dict_value->GetDict()) {
    if (extension_id == kWildcard) {
      continue;
    }
    if (!entry_value.is_dict()) {
      continue;
    }
    const base::DictValue& entry_dict = entry_value.GetDict();
    const std::string* installation_mode =
        entry_dict.FindString(kInstallationMode);
    if (!installation_mode || (*installation_mode != kForceInstalled &&
                               *installation_mode != kNormalInstalled)) {
      continue;
    }
    const std::string* update_url = entry_dict.FindString(kUpdateUrl);
    if (!update_url || base::EqualsCaseInsensitiveASCII(
                           *update_url, kChromeWebstoreUpdateURL)) {
      continue;
    }

    filtered_extensions.push_back(extension_id);
  }

  // Marking the blocked extension by adding the "[BLOCKED]" prefix. This is an
  // invalid extension id and will be removed by PolicyHandler later.
  if (!filtered_extensions.empty()) {
    base::Value filtered_dict = const_dict_value->Clone();
    base::DictValue& policy_dict = filtered_dict.GetDict();
    for (const auto& extension : filtered_extensions) {
      auto setting = policy_dict.Extract(extension);
      if (!setting)
        continue;
      policy_dict.Set(kBlockedExtensionPrefix + extension,
                      std::move(setting.value()));
    }
    map_entry->set_value(std::move(filtered_dict));
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
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
  if (FilterSensitiveExtensionsInstallForcelist(
          policy->GetMutable(key::kExtensionInstallForcelist))) {
    invalid_policies++;
  }
  if (FilterSensitiveExtensionSettings(
          policy->GetMutable(key::kExtensionSettings))) {
    invalid_policies++;
  }
#endif
  for (const char* sensitive_policy : GetSensitivePolicies()) {
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
  for (const char* sensitive_policy : GetSensitivePolicies()) {
    if (sensitive_policy == policy_name) {
      return true;
    }
  }
  return false;
}

}  // namespace policy
