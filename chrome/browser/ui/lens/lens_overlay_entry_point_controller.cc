// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"

#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

namespace lens {

LensOverlayEntryPointController::LensOverlayEntryPointController(
    Browser* browser)
    : browser_(browser) {
  // Observe changes to fullscreen state.
  fullscreen_observation_.Observe(
      browser->exclusive_access_manager()->fullscreen_controller());

  // Observe changes to user's DSE.
  if (auto* const template_url_service =
          TemplateURLServiceFactory::GetForProfile(browser->profile())) {
    template_url_service_observation_.Observe(template_url_service);
  }
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
  UpdateEntryPointsState(/*hide_if_needed=*/false);
}

void LensOverlayEntryPointController::OnTemplateURLServiceChanged() {
  // Possibly add/remove the entrypoints based on the new users default search
  // engine.
  UpdateEntryPointsState(/*hide_if_needed=*/true);
}

void LensOverlayEntryPointController::OnTemplateURLServiceShuttingDown() {
  template_url_service_observation_.Reset();
}

void LensOverlayEntryPointController::UpdateEntryPointsState(
    bool hide_if_needed) {
  const bool enabled = LensOverlayController::IsEnabled(browser_);

  // Update the 3 dot menu entry point.
  browser_->command_controller()->UpdateCommandEnabled(
      IDC_CONTENT_CONTEXT_LENS_OVERLAY, enabled);

  // Update the pinnable toolbar entry point
  if (auto* const toolbar_entry_point = GetToolbarEntrypoint()) {
    toolbar_entry_point->SetEnabled(enabled);
    if (hide_if_needed) {
      toolbar_entry_point->SetVisible(enabled);
    }
  }
}

actions::ActionItem* LensOverlayEntryPointController::GetToolbarEntrypoint() {
  BrowserActions* browser_actions = browser_->browser_actions();
  return actions::ActionManager::Get().FindAction(
      kActionSidePanelShowLensOverlayResults,
      browser_actions->root_action_item());
}
}  // namespace lens
