// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/browser_utils.h"

#include "base/check.h"
#include "build/build_config.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/site_isolation_policy.h"

namespace {

bool IsURLBlocked(const GURL& url, PolicyBlocklistService* service) {
  if (!service) {
    return false;
  }

  policy::URLBlocklist::URLBlocklistState state =
      service->GetURLBlocklistState(url);

  return state == policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

}  // namespace

namespace device_signals {

safe_browsing::SafeBrowsingState GetSafeBrowsingProtectionLevel(
    PrefService* profile_prefs) {
  DCHECK(profile_prefs);
  bool safe_browsing_enabled =
      profile_prefs->GetBoolean(prefs::kSafeBrowsingEnabled);
  bool safe_browsing_enhanced_enabled =
      profile_prefs->GetBoolean(prefs::kSafeBrowsingEnhanced);

  if (safe_browsing_enabled) {
    if (safe_browsing_enhanced_enabled) {
      return safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION;
    } else {
      return safe_browsing::SafeBrowsingState::STANDARD_PROTECTION;
    }
  } else {
    return safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING;
  }
}

std::optional<safe_browsing::PasswordProtectionTrigger>
GetPasswordProtectionWarningTrigger(PrefService* profile_prefs) {
  DCHECK(profile_prefs);
  if (!profile_prefs->HasPrefPath(prefs::kPasswordProtectionWarningTrigger)) {
    return std::nullopt;
  }
  return static_cast<safe_browsing::PasswordProtectionTrigger>(
      profile_prefs->GetInteger(prefs::kPasswordProtectionWarningTrigger));
}

bool GetChromeRemoteDesktopAppBlocked(PolicyBlocklistService* service) {
  DCHECK(service);
  return IsURLBlocked(GURL("https://remotedesktop.google.com"), service) ||
         IsURLBlocked(GURL("https://remotedesktop.corp.google.com"), service);
}

std::optional<std::string> TryGetEnrollmentDomain(
    policy::CloudPolicyManager* manager) {
  policy::CloudPolicyStore* store = nullptr;
  if (manager && manager->core() && manager->core()->store()) {
    store = manager->core()->store();
  }

  if (store && store->has_policy()) {
    const auto* policy = store->policy();
    if (policy->has_managed_by()) {
      return policy->managed_by();
    } else if (policy->has_display_domain()) {
      return policy->display_domain();
    }
  }
  return std::nullopt;
}

bool GetSiteIsolationEnabled() {
  return content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites();
}

}  // namespace device_signals
