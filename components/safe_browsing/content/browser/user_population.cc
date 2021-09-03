// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/user_population.h"

#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/unified_consent/pref_names.h"
#include "components/version_info/version_info.h"

namespace safe_browsing {

ChromeUserPopulation GetUserPopulation(
    PrefService* prefs,
    bool is_incognito,
    bool is_history_sync_enabled,
    bool is_under_advanced_protection,
    const policy::BrowserPolicyConnector* browser_policy_connector,
    absl::optional<size_t> num_profiles,
    absl::optional<size_t> num_loaded_profiles,
    absl::optional<size_t> num_open_profiles) {
  ChromeUserPopulation population;

  if (prefs) {
    if (IsEnhancedProtectionEnabled(*prefs)) {
      population.set_user_population(ChromeUserPopulation::ENHANCED_PROTECTION);
    } else if (IsExtendedReportingEnabled(*prefs)) {
      population.set_user_population(ChromeUserPopulation::EXTENDED_REPORTING);
    } else if (IsSafeBrowsingEnabled(*prefs)) {
      population.set_user_population(ChromeUserPopulation::SAFE_BROWSING);
    }

    population.set_is_mbb_enabled(prefs->GetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  }

  population.set_is_incognito(is_incognito);

  population.set_is_history_sync_enabled(is_history_sync_enabled);

  population.set_is_under_advanced_protection(is_under_advanced_protection);

  population.set_profile_management_status(
      GetProfileManagementStatus(browser_policy_connector));

  if (base::FeatureList::IsEnabled(kBetterTelemetryAcrossReports)) {
    std::string user_agent =
        version_info::GetProductNameAndVersionForUserAgent() + "/" +
        version_info::GetOSType();
    population.set_user_agent(user_agent);

    if (num_profiles)
      population.set_number_of_profiles(*num_profiles);

    if (num_loaded_profiles)
      population.set_number_of_loaded_profiles(*num_loaded_profiles);

    if (num_open_profiles)
      population.set_number_of_open_profiles(*num_open_profiles);
  }

  return population;
}

}  // namespace safe_browsing
