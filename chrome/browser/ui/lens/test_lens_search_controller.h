// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_TEST_LENS_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_TEST_LENS_SEARCH_CONTROLLER_H_

#include "chrome/browser/ui/lens/lens_search_controller.h"

namespace variations {
class VariationsClient;
}  // namespace variations

namespace lens {

class TestLensSearchController : public LensSearchController {
 public:
  explicit TestLensSearchController(tabs::TabInterface* tab)
      : LensSearchController(tab) {}

  std::unique_ptr<LensOverlayController> CreateLensOverlayController(
      tabs::TabInterface* tab,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      ThemeService* theme_service) override;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_TEST_LENS_SEARCH_CONTROLLER_H_
