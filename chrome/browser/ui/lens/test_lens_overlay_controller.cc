// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/test_lens_overlay_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/variations_client.h"

namespace lens {

TestLensOverlayController::TestLensOverlayController(
    tabs::TabInterface* tab,
    LensSearchController* lens_search_controller,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service)
    : LensOverlayController(tab,
                            lens_search_controller,
                            variations_client,
                            identity_manager,
                            pref_service,
                            sync_service,
                            theme_service) {}

}  // namespace lens
