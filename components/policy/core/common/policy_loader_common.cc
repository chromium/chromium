// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_common.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

// The web store url that is the only trusted source for extensions.
const char kExpectedWebStoreUrl[] =
    ";https://clients2.google.com/service/update2/crx";

// String to be prepended to each blocked entry.
const char kBlockedExtensionPrefix[] = "[BLOCKED]";

// List of policies that are considered only if the user is part of a AD domain
// on Windows or managed on the Mac. Please document any new additions in
// policy_templates.json!
// Please keep the list in alphabetical order!
const char* kSensitivePolicies[] = {
    key::kAutoOpenFileTypes,
    key::kChromeCleanupEnabled,
    key::kChromeCleanupReportingEnabled,
    key::kCommandLineFlagSecurityWarningsEnabled,
    key::kDefaultSearchProviderEnabled,
    key::kHomepageIsNewTabPage,
    key::kHomepageLocation,
    key::kMetricsReportingEnabled,
    key::kNewTabPageLocation,
    key::kPasswordProtectionChangePasswordURL,
    key::kPasswordProtectionLoginURLs,
    key::kRestoreOnStartup,
    key::kRestoreOnStartupURLs,
    key::kSafeBrowsingForTrustedSourcesEnabled,
    key::kSafeBrowsingEnabled,
    key::kSafeBrowsingWhitelistDomains,
    key::kSafeBrowsingAllowlistDomains,
};

}  // namespace

void FilterSensitivePolicies(PolicyMap* policy) {
  int invalid_policies = 0;
  const PolicyMap::Entry* map_entry =
      policy->Get(key::kExtensionInstallForcelist);
  if (map_entry && map_entry->value()) {
    const base::ListValue* policy_list_value = nullptr;
    if (!map_entry->value()->GetAsList(&policy_list_value))
      return;

    base::Value filtered_values(base::Value::Type::LIST);
    for (const auto& list_entry : policy_list_value->GetList()) {
      if (!list_entry.is_string())
        continue;
      std::string entry = list_entry.GetString();
      size_t pos = entry.find(';');
      if (pos == std::string::npos)
        continue;
      // Only allow custom update urls in enterprise environments.
      if (!base::LowerCaseEqualsASCII(entry.substr(pos),
                                      kExpectedWebStoreUrl)) {
        entry = kBlockedExtensionPrefix + entry;
        invalid_policies++;
      }

      filtered_values.Append(entry);
    }
    if (invalid_policies) {
      PolicyMap::Entry filtered_entry = map_entry->DeepCopy();
      filtered_entry.set_value(std::move(filtered_values));
      policy->Set(key::kExtensionInstallForcelist, std::move(filtered_entry));

      const PolicyDetails* details =
          GetChromePolicyDetails(key::kExtensionInstallForcelist);
      base::UmaHistogramSparse("EnterpriseCheck.InvalidPolicies", details->id);
    }
  }

  for (const char* sensitive_policy : kSensitivePolicies) {
    if (policy->Get(sensitive_policy)) {
      policy->GetMutable(sensitive_policy)->SetBlocked();
      invalid_policies++;
      const PolicyDetails* details = GetChromePolicyDetails(sensitive_policy);
      base::UmaHistogramSparse("EnterpriseCheck.InvalidPolicies", details->id);
    }
  }

  UMA_HISTOGRAM_COUNTS_1M("EnterpriseCheck.InvalidPoliciesDetected",
                          invalid_policies);
}  // namespace policy

}  // namespace policy
