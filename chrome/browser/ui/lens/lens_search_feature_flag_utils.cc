// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/lens/lens_keyed_service.h"
#include "chrome/browser/ui/lens/lens_keyed_service_factory.h"
#include "chrome/browser/ui/lens/lens_session_metrics_logger.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"

namespace {
constexpr char kUnitedStatesCountryCode[] = "us";
constexpr char kEnglishUSLocale[] = "en-US";

bool IsEnUs() {
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
  return variations_service->GetStoredPermanentCountry() ==
             kUnitedStatesCountryCode &&
         features->application_locale_storage() &&
         features->application_locale_storage()->Get() == kEnglishUSLocale;
}
}  // namespace

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

  // Otherwise, enable it to en-US users in US.
  return IsEnUs();
}

bool IsAimM3Enabled(Profile* profile) {
  // Set whether to use the AIM eligibility service based on the feature flag.
  bool should_use_aim_eligibility_service = base::FeatureList::IsEnabled(
      lens::features::kLensSearchAimM3UseAimEligibility);

  // Since the AIM Eligibility Service is already launched in the US, force it
  // to be used there for M3 US users.
  if (base::FeatureList::IsEnabled(lens::features::kLensSearchAimM3EnUs) &&
      IsEnUs()) {
    should_use_aim_eligibility_service = true;
  }

  if (should_use_aim_eligibility_service) {
    return AimEligibilityService::GenericKillSwitchFeatureCheck(
        AimEligibilityServiceFactory::GetForProfile(profile),
        lens::features::kLensSearchAimM3, lens::features::kLensSearchAimM3EnUs);
  }
  // If not using the AIM eligibility service, then just check the M3 feature
  // flag.
  return base::FeatureList::IsEnabled(lens::features::kLensSearchAimM3);
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

bool DidUserGrantLensOverlayNeededPermissions(PrefService* pref_service) {
  if (!CanSharePageScreenshotWithLensOverlay(pref_service)) {
    return false;
  }
  if (IsLensOverlayContextualSearchboxEnabled()) {
    return CanSharePageContentWithLensOverlay(pref_service);
  }
  return true;
}

void GrantLensOverlayNeededPermissions(PrefService* pref_service) {
  if (lens::IsLensOverlayContextualSearchboxEnabled()) {
    pref_service->SetBoolean(prefs::kLensSharingPageContentEnabled, true);
  }
  pref_service->SetBoolean(prefs::kLensSharingPageScreenshotEnabled, true);
}

bool MaybeIncrementPrivacyNoticeShownCountAndGrantPermissions(
    PrefService* pref_service) {
  // This function should not be called if the non-blocking privacy notice is
  // not enabled.
  CHECK(lens::features::IsLensOverlayNonBlockingPrivacyNoticeEnabled());

  if (DidUserGrantLensOverlayNeededPermissions(pref_service)) {
    // User has already granted permissions. Do not show the privacy notice.
    return true;
  }

  int impression_cap =
      features::GetLensOverlayNonBlockingPrivacyNoticeImpressionCap();
  if (impression_cap <= 0) {
    // No impression cap. The privacy notice should be shown.
    return false;
  }

  int shown_count = pref_service->GetInteger(
      prefs::kLensOverlayNonBlockingPrivacyNoticeShownCount);
  if (shown_count >= impression_cap) {
    // Shown count has reached impression cap. Grant permissions and do not show
    // the privacy notice.
    GrantLensOverlayNeededPermissions(pref_service);
    return true;
  }

  // Shown count has not reached impression cap. Increment the count and show
  // the privacy notice.
  pref_service->SetInteger(
      prefs::kLensOverlayNonBlockingPrivacyNoticeShownCount, shown_count + 1);
  return false;
}

}  // namespace lens
