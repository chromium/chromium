// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/test_lens_search_controller.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/lens/test_lens_overlay_controller.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/variations_client.h"

namespace lens {

std::unique_ptr<LensOverlayController>
TestLensSearchController::CreateLensOverlayController(
    tabs::TabInterface* tab,
    LensSearchController* lens_search_controller,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service) {
  // Set browser color scheme to light mode for consistency.
  theme_service->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);

  return std::make_unique<TestLensOverlayController>(
      tab, lens_search_controller, variations_client, identity_manager,
      pref_service, sync_service, theme_service);
}

}  // namespace lens
