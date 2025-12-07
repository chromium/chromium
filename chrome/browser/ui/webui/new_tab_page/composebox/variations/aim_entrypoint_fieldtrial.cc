// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/aim_entrypoint_fieldtrial.h"

#include "base/feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "components/omnibox/browser/aim_eligibility_service.h"

namespace ntp_composebox {

BASE_FEATURE(kNtpSearchboxComposeEntrypoint, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNtpSearchboxComposeEntrypointEnglishUs,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNtpSearchboxComposeEntrypointEnabled(Profile* profile) {
  return AimEligibilityService::GenericKillSwitchFeatureCheck(
      AimEligibilityServiceFactory::GetForProfile(profile),
      kNtpSearchboxComposeEntrypoint, kNtpSearchboxComposeEntrypointEnglishUs);
}

}  // namespace ntp_composebox
