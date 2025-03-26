// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/tab_collections/public/tab_interface.h"

class LensOverlayController;

namespace variations {
class VariationsClient;
}  // namespace variations

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

class PrefService;
class ThemeService;

// Controller for all Lens Search features in Chrome. All external entry points
// should go through this controller.
// This migration is still in progress. Follow progress via crbug.com/404941800.
class LensSearchController {
 public:
  explicit LensSearchController(tabs::TabInterface* tab);
  virtual ~LensSearchController();

  // Initializes all the necessary dependencies for the LensSearchController.
  void Initialize(variations::VariationsClient* variations_client,
                  signin::IdentityManager* identity_manager,
                  PrefService* pref_service,
                  syncer::SyncService* sync_service,
                  ThemeService* theme_service);

  // Returns the LensOverlayController.
  LensOverlayController* lens_overlay_controller();

 protected:
  // Override these methods to stub out individual feature controllers for
  // testing.
  virtual std::unique_ptr<LensOverlayController> CreateLensOverlayController(
      tabs::TabInterface* tab,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      ThemeService* theme_service);

 private:
  // Whether the LensSearchController has been initialized.
  bool initialized_ = false;

  std::unique_ptr<LensOverlayController> lens_overlay_controller_;

  // Owns this class.
  raw_ptr<tabs::TabInterface> tab_;
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_
