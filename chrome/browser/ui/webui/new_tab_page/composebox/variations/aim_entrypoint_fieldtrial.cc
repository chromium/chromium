// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/aim_entrypoint_fieldtrial.h"

#include "base/feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"

namespace ntp_composebox {

BASE_FEATURE(kNtpSearchboxComposeEntrypoint,
             "NtpSearchboxComposeEntrypoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNtpSearchboxComposeEntrypointEnglishUs,
             "NtpSearchboxComposeEntrypointEnglishUs",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNtpSearchboxComposeEntrypointEnabled(Profile* profile) {
  // If the generic entrypoint feature is overridden to be false, return false.
  auto* feature_list = base::FeatureList::GetInstance();
  if (feature_list &&
      feature_list->IsFeatureOverridden(kNtpSearchboxComposeEntrypoint.name) &&
      !base::FeatureList::IsEnabled(kNtpSearchboxComposeEntrypoint)) {
    return false;
  }

  const auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);

  // If the server eligibility is enabled, check overall eligibility alone.
  // The service will control locale rollout so there's no need to check locale
  // or the state of kNtpSearchboxComposeEntrypoint(EnglishUs) below.
  if (aim_eligibility_service->IsServerEligibilityEnabled()) {
    return aim_eligibility_service->IsAimEligible();
  }

  // If not locally eligible, return false.
  if (!aim_eligibility_service->IsAimLocallyEligible()) {
    return false;
  }

  // For English locales in the US, check the EnglishUS entrypoint feature.
  // Since the base::Feature default state cannot be set based on a dynamic
  // condition, use a dedicated feature to control the entrypoint for English
  // locales in the US, allowing for targeted rollbacks if needed.
  if (AimEligibilityService::IsCountry("us") &&
      AimEligibilityService::IsLanguage("en")) {
    return base::FeatureList::IsEnabled(
        kNtpSearchboxComposeEntrypointEnglishUs);
  }

  // Otherwise, check the generic entrypoint feature.
  return base::FeatureList::IsEnabled(kNtpSearchboxComposeEntrypoint);
}

}  // namespace ntp_composebox
