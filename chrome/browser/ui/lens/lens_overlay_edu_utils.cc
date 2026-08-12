// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_edu_utils.h"

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_keyed_service.h"
#include "chrome/browser/ui/lens/lens_keyed_service_factory.h"
#include "components/lens/lens_features.h"

namespace lens {

bool ShouldShowLensOverlayEduActionChip(Profile* profile) {
  if (!lens::features::IsLensOverlayEduActionChipEnabled()) {
    return false;
  }

  LensKeyedService* service = LensKeyedServiceFactory::GetForProfile(
      profile, /*create_if_necessary=*/true);
  if (service == nullptr) {
    return false;
  }

  if (service->GetActionChipShownCount() >
      lens::features::GetLensOverlayEduActionChipMaxShownCount()) {
    return false;
  }

  base::TimeDelta time_delta =
      base::Time::Now() - service->GetActionChipLastShownTime();
  // This function may be called multiple times for a single show. Check that
  // the debounce interval has passed before considering the current call a
  // second show attempt.
  if (time_delta >=
          lens::features::GetLensOverlayEduActionChipShowDebounceInterval() &&
      time_delta < lens::features::GetLensOverlayEduActionChipShowInterval()) {
    return false;
  }
  return true;
}

void RecordLensOverlayEduActionChipShown(Profile* profile) {
  LensKeyedService* service = LensKeyedServiceFactory::GetForProfile(
      profile, /*create_if_necessary=*/true);
  if (service == nullptr) {
    return;
  }
  service->IncrementActionChipShownCount();
  service->ResetActionChipLastShownTime();
}

}  // namespace lens
