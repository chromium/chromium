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
      LensSearchController* lens_search_controller,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      ThemeService* theme_service) override;

  std::unique_ptr<lens::LensOverlayQueryController> CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlayInteractionResponseCallback interaction_callback,
      lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      lens::UploadProgressCallback upload_progress_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller) override;

  std::unique_ptr<lens::LensSearchContextualizationController>
  CreateLensSearchContextualizationController() override;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_TEST_LENS_SEARCH_CONTROLLER_H_
