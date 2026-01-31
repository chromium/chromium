// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_results_panel_router.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"

namespace {

contextual_tasks::ContextualTasksUiService* GetContextualTasksUiService(
    Profile* profile) {
  return contextual_tasks::ContextualTasksUiServiceFactory::
      GetForBrowserContext(profile);
}
}  // namespace

namespace lens {

LensResultsPanelRouter::LensResultsPanelRouter(
    Profile* profile,
    LensSearchController* lens_search_controller)
    : lens_search_controller_(lens_search_controller), profile_(profile) {}

LensResultsPanelRouter::~LensResultsPanelRouter() = default;

bool LensResultsPanelRouter::IsEntryShowing() {
  // If Lens in contextual tasks is enabled, the side panel to check is the
  // contextual tasks panel.
  if (lens_search_controller_->should_route_to_contextual_tasks()) {
    return tab_interface()
        ->GetBrowserWindowInterface()
        ->GetFeatures()
        .side_panel_ui()
        ->IsSidePanelEntryShowing(
            SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
  }

  return lens_side_panel_coordinator()->IsEntryShowing();
}

SidePanelEntry::PanelType LensResultsPanelRouter::GetPanelType() const {
  if (lens_search_controller_->should_route_to_contextual_tasks()) {
    return SidePanelEntry::PanelType::kToolbar;
  }

  return lens_side_panel_coordinator()->GetPanelType();
}

void LensResultsPanelRouter::FocusSearchbox() {
  lens_side_panel_coordinator()->FocusSearchbox();
}

void LensResultsPanelRouter::OnOverlayShown() {
  if (lens_search_controller_->should_route_to_contextual_tasks()) {
    if (auto* service = GetContextualTasksUiService(profile_)) {
      service->OnLensOverlayStateChanged(
          lens_search_controller_->GetTabInterface()
              ->GetBrowserWindowInterface(),
          /*is_showing=*/true);
    }
    return;
  }
  lens_side_panel_coordinator()->SetIsOverlayShowing(true);
}

void LensResultsPanelRouter::OnOverlayHidden() {
  if (lens_search_controller_->should_route_to_contextual_tasks()) {
    if (auto* service = GetContextualTasksUiService(profile_)) {
      service->OnLensOverlayStateChanged(
          lens_search_controller_->GetTabInterface()
              ->GetBrowserWindowInterface(),
          /*is_showing=*/false);
    }
    return;
  }
  lens_side_panel_coordinator()->SetIsOverlayShowing(false);
}

}  // namespace lens
