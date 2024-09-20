// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_permissions_util.h"

#include <memory>

#include "base/feature_list.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "google_apis/google_api_keys.h"

namespace {

bool IsUserConsentedToAnonymousDataCollectionAndAllowedToFetchFromRemoteService(
    PrefService* pref_service) {
  if (!optimization_guide::features::
          IsRemoteFetchingForAnonymousDataConsentEnabled()) {
    return false;
  }

  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(pref_service);
  return helper->IsEnabled();
}

}  // namespace

namespace optimization_guide {

bool IsUserPermittedToFetchFromRemoteOptimizationGuide(
    bool is_off_the_record,
    PrefService* pref_service) {
  if (is_off_the_record)
    return false;

  if (switches::ShouldOverrideCheckingUserPermissionsToFetchHintsForTesting()) {
    return true;
  }

  if (!features::IsRemoteFetchingEnabled()) {
    return false;
  }

  if (!switches::ShouldSkipGoogleApiKeyConfigurationCheck() &&
      !google_apis::HasAPIKeyConfigured()) {
    return false;
  }

  return IsUserConsentedToAnonymousDataCollectionAndAllowedToFetchFromRemoteService(
      pref_service);
}

}  // namespace optimization_guide
