// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/policy_engine.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/service/variations_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#endif

namespace {

// Enum representing reasons for the real time URL lookup to
// choose the consumer version instead of the enterprise version.
enum class ConsumerVersionReason {
  // The total number of checks. This value should be used as the denominator
  // when calculating the percentage of a specific reason below.
  TOTAL_CHECKS = 0,
  // IS_OFF_THE_RECORD = 1, deprecated.
  INVALID_DM_TOKEN = 2,
  POLICY_DISABLED = 3,
  IS_INCOGNITO = 4,

  kMaxValue = IS_INCOGNITO
};

}  // namespace

namespace safe_browsing {

// static
bool RealTimePolicyEngine::IsInExcludedCountry(
    const std::string& country_code) {
  return base::Contains(GetExcludedCountries(), country_code);
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
bool RealTimePolicyEngine::CanPerformFullURLLookup(
    PrefService* pref_service,
    bool is_off_the_record,
    variations::VariationsService* variations_service) {
  if (is_off_the_record)
    return false;

  // |variations_service| can be nullptr in tests.
  if (variations_service &&
      IsInExcludedCountry(variations_service->GetLatestCountry())) {
    return false;
  }

  return IsUserEpOptedIn(pref_service) || IsUserMbbOptedIn(pref_service);
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookupWithToken(
    PrefService* pref_service,
    bool is_off_the_record,
    ClientConfiguredForTokenFetchesCallback client_callback,
    variations::VariationsService* variations_service) {
  if (!CanPerformFullURLLookup(pref_service, is_off_the_record,
                               variations_service)) {
    return false;
  }

  return std::move(client_callback).Run(IsUserEpOptedIn(pref_service));
}

// static
bool RealTimePolicyEngine::CanPerformEnterpriseFullURLLookup(
    const PrefService* pref_service,
    bool has_valid_dm_token,
    bool is_off_the_record,
    bool is_guest_profile) {
  base::UmaHistogramEnumeration("SafeBrowsing.RT.ConsumerVersionReason",
                                ConsumerVersionReason::TOTAL_CHECKS);

  if (is_off_the_record && !is_guest_profile) {
    base::UmaHistogramEnumeration("SafeBrowsing.RT.ConsumerVersionReason",
                                  ConsumerVersionReason::IS_INCOGNITO);
    return false;
  }

  if (!has_valid_dm_token) {
    base::UmaHistogramEnumeration("SafeBrowsing.RT.ConsumerVersionReason",
                                  ConsumerVersionReason::INVALID_DM_TOKEN);
    return false;
  }

  if (pref_service->GetInteger(
          enterprise_connectors::kEnterpriseRealTimeUrlCheckMode) !=
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED) {
    base::UmaHistogramEnumeration("SafeBrowsing.RT.ConsumerVersionReason",
                                  ConsumerVersionReason::POLICY_DISABLED);
    return false;
  }

  return true;
}

}  // namespace safe_browsing
