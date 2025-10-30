// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "content/public/browser/web_contents.h"

DEFINE_USER_DATA(ReadAnythingController);

ReadAnythingController* ReadAnythingController::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

ReadAnythingController::ReadAnythingController(tabs::TabInterface* tab)
    : tab_(tab),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

ReadAnythingController::~ReadAnythingController() = default;

// TODO(crbug.com/447418049): Open immersive reading mode via this
// entrypoint. Currently just open side panel reading mode via
// ReadAnythingController when is_immersive_read_anything_enabled_ flag is
// enabled.
void ReadAnythingController::ShowUI(SidePanelOpenTrigger trigger) {
  CHECK(tab_);
  // The UI should only be shown for the active tab.
  CHECK(tab_->IsActivated());
  if (!tab_->GetBrowserWindowInterface()) {
    return;
  }
  auto* side_panel_ui =
      tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_ui();
  if (!side_panel_ui) {
    return;
  }

  side_panel_ui->Show(SidePanelEntryId::kReadAnything, trigger);
}

void ReadAnythingController::InvokePageAction(
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
  bwi->GetFeatures().side_panel_ui()->Toggle(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything),
      side_panel_open_trigger);
}
