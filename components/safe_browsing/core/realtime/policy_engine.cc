// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/realtime/policy_engine.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/service/variations_service.h"

#if defined(OS_ANDROID)
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#endif

namespace safe_browsing {

// static
bool RealTimePolicyEngine::IsInExcludedCountry(
    const std::string& country_code) {
  return base::Contains(GetExcludedCountries(), country_code);
}

// static
bool RealTimePolicyEngine::IsUrlLookupEnabled() {
  return base::FeatureList::IsEnabled(kRealTimeUrlLookupEnabled);
}

// static
bool RealTimePolicyEngine::IsUrlLookupEnabledForEp() {
  return base::FeatureList::IsEnabled(kRealTimeUrlLookupEnabledForEP);
}

// static
bool RealTimePolicyEngine::IsUrlLookupEnabledForEpWithToken() {
  return base::FeatureList::IsEnabled(kRealTimeUrlLookupEnabledForEPWithToken);
}

// static
bool RealTimePolicyEngine::IsUserMbbOptedIn(PrefService* pref_service) {
  return pref_service->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

// static
bool RealTimePolicyEngine::IsUserEpOptedIn(PrefService* pref_service) {
  return IsEnhancedProtectionEnabled(*pref_service);
}

// static
bool RealTimePolicyEngine::IsPrimaryAccountSignedIn(
    signin::IdentityManager* identity_manager) {
  CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo(
          signin::ConsentLevel::kNotRequired);
  return !primary_account_info.account_id.empty();
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookup(
    PrefService* pref_service,
    bool is_off_the_record,
    variations::VariationsService* variations_service) {
  if (is_off_the_record)
    return false;

  // |variations_service| can be nullptr in tests.
  if (variations_service &&
      IsInExcludedCountry(variations_service->GetStoredPermanentCountry()))
    return false;

  if (IsUrlLookupEnabledForEp() && IsUserEpOptedIn(pref_service))
    return true;

  return IsUrlLookupEnabled() && IsUserMbbOptedIn(pref_service);
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookupWithToken(
    PrefService* pref_service,
    bool is_off_the_record,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    variations::VariationsService* variations_service) {
  if (!CanPerformFullURLLookup(pref_service, is_off_the_record,
                               variations_service)) {
    return false;
  }

  if (IsUrlLookupEnabledForEp() && IsUserEpOptedIn(pref_service) &&
      IsUrlLookupEnabledForEpWithToken() &&
      IsPrimaryAccountSignedIn(identity_manager)) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(kRealTimeUrlLookupEnabledWithToken)) {
    return false;
  }

  // |sync_service| can be null in Incognito, and also be set to null by a
  // cmdline param.
  if (!sync_service) {
    return false;
  }

  // Full URL lookup with token is enabled when the user is syncing their
  // browsing history without a custom passphrase.
  return syncer::GetUploadToGoogleState(
             sync_service, syncer::ModelType::HISTORY_DELETE_DIRECTIVES) ==
             syncer::UploadState::ACTIVE &&
         !sync_service->GetUserSettings()->IsUsingSecondaryPassphrase();
}

// static
bool RealTimePolicyEngine::CanPerformEnterpriseFullURLLookup(
    const PrefService* pref_service,
    bool has_valid_dm_token,
    bool is_off_the_record) {
  if (is_off_the_record) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(kRealTimeUrlLookupEnabledForEnterprise)) {
    return false;
  }

  if (!has_valid_dm_token) {
    return false;
  }

  return pref_service->GetInteger(
             prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode) ==
         REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED;
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookupForResourceType(
    ResourceType resource_type,
    bool can_rt_check_subresource_url) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.ResourceTypes.Requested",
                            resource_type);
  if (resource_type == ResourceType::kMainFrame) {
    return true;
  }
  if (resource_type == ResourceType::kSubFrame &&
      can_rt_check_subresource_url &&
      base::FeatureList::IsEnabled(
          kRealTimeUrlLookupNonMainframeEnabledForEP)) {
    return true;
  }
  return false;
}

}  // namespace safe_browsing
