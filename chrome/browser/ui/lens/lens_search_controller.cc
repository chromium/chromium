// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_controller.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"

LensSearchController::LensSearchController(tabs::TabInterface* tab)
    : tab_(tab) {}
LensSearchController::~LensSearchController() = default;

void LensSearchController::Initialize(
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service) {
  CHECK(!initialized_);
  initialized_ = true;

  lens_overlay_controller_ = CreateLensOverlayController(
      tab_, this, variations_client, identity_manager, pref_service,
      sync_service, theme_service);

  lens_overlay_side_panel_coordinator_ =
      CreateLensOverlaySidePanelCoordinator();
}

tabs::TabInterface* LensSearchController::GetTabInterface() {
  return tab_;
}

LensOverlayController* LensSearchController::lens_overlay_controller() {
  CHECK(initialized_)
      << "The LensSearchController has not been initialized. Initialize() must "
         "be called before using the LensSearchController.";
  return lens_overlay_controller_.get();
}

lens::LensOverlaySidePanelCoordinator*
LensSearchController::lens_overlay_side_panel_coordinator() {
  CHECK(initialized_)
      << "The LensSearchController has not been initialized. Initialize() must "
         "be called before using the LensSearchController.";
  return lens_overlay_side_panel_coordinator_.get();
}

std::unique_ptr<LensOverlayController>
LensSearchController::CreateLensOverlayController(
    tabs::TabInterface* tab,
    LensSearchController* lens_search_controller,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service) {
  return std::make_unique<LensOverlayController>(
      tab, lens_search_controller, variations_client, identity_manager,
      pref_service, sync_service, theme_service);
}

std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
LensSearchController::CreateLensOverlaySidePanelCoordinator() {
  return std::make_unique<lens::LensOverlaySidePanelCoordinator>(this);
}
