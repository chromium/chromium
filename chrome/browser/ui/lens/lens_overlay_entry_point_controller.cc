// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"

#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"

namespace lens {

LensOverlayEntryPointController::LensOverlayEntryPointController(
    Browser* browser)
    : browser_(browser) {
  fullscreen_observation_.Observe(
      browser->exclusive_access_manager()->fullscreen_controller());
}

LensOverlayEntryPointController::~LensOverlayEntryPointController() {
  // Don't leave the reference pointer dangling.
  browser_ = nullptr;
}

void LensOverlayEntryPointController::OnFullscreenStateChanged() {
  // Disable the Lens entry points in the top chrome if there is no top bar in
  // Chrome. On Mac and ChromeOS, it is possible to hover over the top of the
  // screen to get the top bar back, but since does top bar does not stay
  // open, we need to disable those entry points.
  UpdateEntryPointsState();
}

void LensOverlayEntryPointController::UpdateEntryPointsState() {
  bool enabled = LensOverlayController::IsEnabled(browser_);

  // Update the 3 dot menu entry point.
  browser_->command_controller()->UpdateCommandEnabled(
      IDC_CONTENT_CONTEXT_LENS_OVERLAY, enabled);

  // Update the pinnable toolbar entry point
  BrowserActions* browser_actions = browser_->browser_actions();
  auto* action = actions::ActionManager::Get().FindAction(
      kActionSidePanelShowLensOverlayResults,
      browser_actions->root_action_item());
  if (!action) {
    return;
  }
  action->SetEnabled(enabled);
}
}  // namespace lens
