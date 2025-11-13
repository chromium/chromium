// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
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
    auto* const user_ed = BrowserUserEducationInterface::From(bwi);
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHReadingModePageActionLabelFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
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
void ReadAnythingEntryPointController::ShowUI(
    BrowserWindowInterface* bwi,
    ReadAnythingOpenTrigger open_trigger) {
  if (!bwi) {
    return;
  }

  SidePanelOpenTrigger side_panel_open_trigger;
  switch (open_trigger) {
    case ReadAnythingOpenTrigger::kAppMenu:
      side_panel_open_trigger = SidePanelOpenTrigger::kAppMenu;
      break;
    case ReadAnythingOpenTrigger::kReadAnythingContextMenu:
      side_panel_open_trigger = SidePanelOpenTrigger::kReadAnythingContextMenu;
      break;
    case ReadAnythingOpenTrigger::kReadAnythingNavigationThrottle:
      side_panel_open_trigger =
          SidePanelOpenTrigger::kReadAnythingNavigationThrottle;
      break;
  }

  if (features::IsImmersiveReadAnythingEnabled()) {
    if (tabs::TabInterface* tab = bwi->GetActiveTabInterface()) {
      auto* controller = ReadAnythingController::From(tab);
      CHECK(controller);
      controller->ShowUI(side_panel_open_trigger);
    }
  } else {
    bwi->GetFeatures().side_panel_ui()->Show(
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
  auto* const user_ed = BrowserUserEducationInterface::From(bwi);
  // No need to show the chip if reading mode is already open.
  // TODO(crbug.com/447418049): Check for immersive reading mode here too.
  if (should_show_page_action && !IsReadAnythingEntryShowing(bwi)) {
    page_action_controller->Show(kActionSidePanelShowReadAnything);
    page_action_controller->ShowSuggestionChip(
        kActionSidePanelShowReadAnything);
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHReadingModePageActionLabelFeature);
  } else {
    user_ed->AbortFeaturePromo(
        feature_engagement::kIPHReadingModePageActionLabelFeature);
    page_action_controller->Hide(kActionSidePanelShowReadAnything);
  }
}

}  // namespace read_anything
