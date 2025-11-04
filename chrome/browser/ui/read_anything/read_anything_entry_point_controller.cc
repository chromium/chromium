// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "ui/accessibility/accessibility_features.h"

namespace read_anything {

// static
void ReadAnythingEntryPointController::InvokePageAction(
    BrowserWindowInterface* bwi,
    const actions::ActionInvocationContext& context) {
  if (!bwi) {
    return;
  }

  std::underlying_type_t<page_actions::PageActionTrigger> page_action_trigger =
      context.GetProperty(page_actions::kPageActionTriggerKey);
  SidePanelOpenTrigger side_panel_open_trigger;
  if (page_action_trigger == page_actions::kInvalidPageActionTrigger) {
    side_panel_open_trigger = SidePanelOpenTrigger::kPinnedEntryToolbarButton;
  } else {
    side_panel_open_trigger = SidePanelOpenTrigger::kReadAnythingOmniboxChip;
  }

  // TODO(crbug.com/447418049): Open immersive reading mode via this entrypoint.
  // TODO(crbug.com/455640523): Finalize the behavior here once UX & PM are
  // aligned. This may only open and not close RM, or it may trigger a LHS chip
  // after opening RM.
  if (features::IsImmersiveReadAnythingEnabled()) {
    if (tabs::TabInterface* tab = bwi->GetActiveTabInterface()) {
      auto* controller = ReadAnythingController::From(tab);
      CHECK(controller);
      controller->ToggleReadAnythingSidePanel(side_panel_open_trigger);
    }
  } else {
    bwi->GetFeatures().side_panel_ui()->Toggle(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything),
        side_panel_open_trigger);
  }
}

// static
void ReadAnythingEntryPointController::UpdatePageActionVisibility(
    bool should_show_page_action,
    BrowserWindowInterface* bwi) {
  if (!base::FeatureList::IsEnabled(features::kPageActionsMigration) ||
      !features::IsReadAnythingOmniboxChipEnabled()) {
    return;
  }

  page_actions::PageActionController* page_action_controller =
      bwi->GetActiveTabInterface()->GetTabFeatures()->page_action_controller();
  if (should_show_page_action) {
    page_action_controller->Show(kActionSidePanelShowReadAnything);
    page_action_controller->ShowSuggestionChip(
        kActionSidePanelShowReadAnything);
  } else {
    page_action_controller->Hide(kActionSidePanelShowReadAnything);
  }
}

}  // namespace read_anything
