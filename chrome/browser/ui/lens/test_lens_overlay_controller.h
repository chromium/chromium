// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_CONTROLLER_H_

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

namespace lens {

// Stubs out network requests.
class TestLensOverlayController : public LensOverlayController {
 public:
  TestLensOverlayController(tabs::TabInterface* tab,
                            LensSearchController* lens_search_controller,
                            variations::VariationsClient* variations_client,
                            signin::IdentityManager* identity_manager,
                            PrefService* pref_service,
                            syncer::SyncService* sync_service,
                            ThemeService* theme_service);
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_CONTROLLER_H_
