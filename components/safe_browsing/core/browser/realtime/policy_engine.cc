// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/policy_engine.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/service/variations_service.h"
#include "services/network/public/cpp/request_destination.h"

#if BUILDFLAG(IS_ANDROID)
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
      IsInExcludedCountry(variations_service->GetStoredPermanentCountry()))
    return false;

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
    bool is_off_the_record) {
  if (is_off_the_record) {
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
bool RealTimePolicyEngine::CanPerformFullURLLookupForRequestDestination(
    network::mojom::RequestDestination request_destination,
    bool can_rt_check_subresource_url) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.RequestDestinations.Requested",
                            request_destination);
  if (request_destination == network::mojom::RequestDestination::kDocument) {
    return true;
  }
  if (network::IsRequestDestinationEmbeddedFrame(request_destination) &&
      can_rt_check_subresource_url) {
    return true;
  }
  return false;
}

}  // namespace safe_browsing
