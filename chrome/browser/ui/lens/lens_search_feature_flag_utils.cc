// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"

#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/lens/lens_keyed_service.h"
#include "chrome/browser/ui/lens/lens_keyed_service_factory.h"
#include "chrome/browser/ui/lens/lens_session_metrics_logger.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/variations/service/variations_service.h"

namespace lens {

bool IsLensOverlayContextualSearchboxEnabled() {
  // If the feature is overridden (e.g. via server-side config or command-line),
  // use that state.
  auto* feature_list = base::FeatureList::GetInstance();
  if (feature_list &&
      (feature_list->IsFeatureOverridden(
          lens::features::kLensOverlayContextualSearchbox.name))) {
    // Important: If a server-side config applies to this client (i.e. after
    // accounting for its filters), but the client gets assigned to the default
    // group, they will still take this code path and receive the state
    // specified via BASE_FEATURE() above.
    return base::FeatureList::IsEnabled(
        lens::features::kLensOverlayContextualSearchbox);
  }

  // Safety check since this is a CP'd change.
  if (!g_browser_process) {
    DCHECK(g_browser_process) << "g_browser_process is null";
    return false;
  }

  // VariationsService and Features should exist.
  auto* variations_service = g_browser_process->variations_service();
  auto* features = g_browser_process->GetFeatures();

  // Safety check since this is a CP'd change.
  if (!variations_service || !features) {
    return false;
  }

  // Otherwise, enable it in the US to en-US locales via client-side code.
  return variations_service->GetStoredPermanentCountry() == "us" &&
         features->application_locale_storage() &&
         features->application_locale_storage()->Get() == "en-US";
}

bool IsAimM3Enabled(Profile* profile) {
  return AimEligibilityService::GenericKillSwitchFeatureCheck(
      AimEligibilityServiceFactory::GetForProfile(profile),
      lens::features::kLensSearchAimM3, lens::features::kLensSearchAimM3EnUs);
}

bool ShouldShowLensOverlayEduActionChip(Profile* profile) {
  LensKeyedService* service = LensKeyedServiceFactory::GetForProfile(
      profile, /*create_if_necessary=*/true);
  if (service == nullptr) {
    return false;
  }
  int shown_count = service->GetActionChipShownCount();
  return lens::features::IsLensOverlayEduActionChipEnabled() &&
         shown_count <=
             lens::features::GetLensOverlayEduActionChipMaxShownCount();
}

void IncrementLensOverlayEduActionChipShownCount(Profile* profile) {
  LensKeyedService* service = LensKeyedServiceFactory::GetForProfile(
      profile, /*create_if_necessary=*/true);
  service->IncrementActionChipShownCount();
}

}  // namespace lens
