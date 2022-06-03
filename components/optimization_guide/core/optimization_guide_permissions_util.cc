// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_permissions_util.h"

#include <memory>

#include "base/feature_list.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace {

bool IsUserDataSaverEnabledAndAllowedToFetchFromRemoteService(
    bool is_off_the_record,
    PrefService* pref_service) {
  // Check if they are a data saver user.
  return data_reduction_proxy::DataReductionProxySettings::
      IsDataSaverEnabledByUser(is_off_the_record, pref_service);
}

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

  if (!features::IsRemoteFetchingEnabled())
    return false;

  if (features::IsRemoteFetchingExplicitlyAllowedForPerformanceInfo())
    return true;

  if (IsUserDataSaverEnabledAndAllowedToFetchFromRemoteService(
          is_off_the_record, pref_service))
    return true;

  return IsUserConsentedToAnonymousDataCollectionAndAllowedToFetchFromRemoteService(
      pref_service);
}

}  // namespace optimization_guide
