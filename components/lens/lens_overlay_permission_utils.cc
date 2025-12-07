// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_overlay_permission_utils.h"

#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace lens {

namespace prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      kLensOverlaySettings,
      static_cast<int>(LensOverlaySettingsPolicyValue::kEnabled));

  registry->RegisterBooleanPref(kLensSharingPageScreenshotEnabled, false);
  registry->RegisterBooleanPref(kLensSharingPageContentEnabled, false);
}

}  // namespace prefs

bool CanSharePageScreenshotWithLensOverlay(PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kLensSharingPageScreenshotEnabled);
}

bool CanSharePageContentWithLensOverlay(PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kLensSharingPageContentEnabled);
}

bool CanSharePageURLWithLensOverlay(PrefService* pref_service) {
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(pref_service);
  return helper->IsEnabled() ||
         CanSharePageContentWithLensOverlay(pref_service);
}

bool CanSharePageTitleWithLensOverlay(syncer::SyncService* sync_service,
                                      PrefService* pref_service) {
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewPersonalizedDataCollectionConsentHelper(sync_service);
  return helper->IsEnabled() ||
         CanSharePageContentWithLensOverlay(pref_service);
}

}  // namespace lens
