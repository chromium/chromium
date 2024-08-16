// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"

#include "base/system/sys_info.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"

namespace lens {

LensOverlayEntryPointController::LensOverlayEntryPointController() = default;

void LensOverlayEntryPointController::Initialize(
    BrowserWindowInterface* browser_window_interface,
    CommandUpdater* command_updater) {
  browser_window_interface_ = browser_window_interface;
  command_updater_ = command_updater;

  // Observe changes to fullscreen state.
  fullscreen_observation_.Observe(
      browser_window_interface_->GetExclusiveAccessManager()
          ->fullscreen_controller());

  // Observe changes to user's DSE.
  if (auto* const template_url_service =
          TemplateURLServiceFactory::GetForProfile(
              browser_window_interface_->GetProfile())) {
    template_url_service_observation_.Observe(template_url_service);
  }

  // Update all entry points.
  UpdateEntryPointsState(/*hide_if_needed=*/true);
}

LensOverlayEntryPointController::~LensOverlayEntryPointController() = default;

bool LensOverlayEntryPointController::IsEnabled() {
  // This class is initialized if and only if it is observing.
  if (!fullscreen_observation_.IsObserving()) {
    return false;
  }

  // Feature is disabled via finch.
  if (!lens::features::IsLensOverlayEnabled()) {
    return false;
  }

  // Disable in fullscreen without top-chrome.
  if (!lens::features::GetLensOverlayEnableInFullscreen() &&
      browser_window_interface_->GetExclusiveAccessManager()
          ->context()
          ->IsFullscreen() &&
      !browser_window_interface_->IsTabStripVisible()) {
    return false;
  }

  // Lens Overlay is disabled via enterprise policy.
  lens::prefs::LensOverlaySettingsPolicyValue policy_value =
      static_cast<lens::prefs::LensOverlaySettingsPolicyValue>(
          browser_window_interface_->GetProfile()->GetPrefs()->GetInteger(
              lens::prefs::kLensOverlaySettings));
  if (policy_value == lens::prefs::LensOverlaySettingsPolicyValue::kDisabled) {
    return false;
  }

  // Lens Overlay is only enabled if the user's default search engine is Google.
  if (lens::features::IsLensOverlayGoogleDseRequired() &&
      !search::DefaultSearchProviderIsGoogle(
          browser_window_interface_->GetProfile())) {
    return false;
  }

  // Finally, only enable the overlay if user meets our minimum RAM requirement.
  static int phys_mem_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  return phys_mem_mb > lens::features::GetLensOverlayMinRamMb();
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
  const bool enabled = IsEnabled();

  // Update the 3 dot menu entry point.
  command_updater_->UpdateCommandEnabled(IDC_CONTENT_CONTEXT_LENS_OVERLAY,
                                         enabled);

  // Update the pinnable toolbar entry point
  if (auto* const toolbar_entry_point = GetToolbarEntrypoint()) {
    toolbar_entry_point->SetEnabled(enabled);
    if (hide_if_needed) {
      toolbar_entry_point->SetVisible(enabled);
    }
  }
}

actions::ActionItem* LensOverlayEntryPointController::GetToolbarEntrypoint() {
  return actions::ActionManager::Get().FindAction(
      kActionSidePanelShowLensOverlayResults,
      /*scope=*/browser_window_interface_->GetActions()->root_action_item());
}
}  // namespace lens
