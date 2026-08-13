// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_permissions_util.h"

#include <memory>

#include "base/command_line.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "google_apis/google_api_keys.h"

namespace {

bool IsUserConsentedToAnonymousDataCollectionAndAllowedToFetchFromRemoteService(
    PrefService* pref_service) {
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(pref_service);
  return helper->IsEnabled();
}

bool ShouldOverrideCheckingUserPermissionsToFetchHintsForTesting() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
      optimization_guide::kDisableCheckingUserPermissionsForTestingSwitch);
}

}  // namespace

namespace optimization_guide {

bool ShouldSkipGoogleApiKeyConfigurationCheck() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
      kGoogleApiKeyConfigurationCheckOverrideSwitch);
}

bool IsUserPermittedToFetchFromRemoteOptimizationGuide(
    bool is_off_the_record,
    PrefService* pref_service) {
  if (is_off_the_record)
    return false;

  if (ShouldOverrideCheckingUserPermissionsToFetchHintsForTesting()) {
    return true;
  }

  if (!ShouldSkipGoogleApiKeyConfigurationCheck() &&
      !google_apis::HasAPIKeyConfigured()) {
    return false;
  }

  return IsUserConsentedToAnonymousDataCollectionAndAllowedToFetchFromRemoteService(
      pref_service);
}

}  // namespace optimization_guide
