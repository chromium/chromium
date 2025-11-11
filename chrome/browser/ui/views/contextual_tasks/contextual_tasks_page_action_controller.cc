// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_page_action_controller.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "components/tabs/public/tab_interface.h"

DEFINE_USER_DATA(ContextualTasksPageActionController);

ContextualTasksPageActionController::ContextualTasksPageActionController(
    tabs::TabInterface* tab_interface)
    : tab_interface_(tab_interface),
      scoped_unowned_user_data_(tab_interface->GetUnownedUserDataHost(),
                                *this) {
  tab_interface->GetTabFeatures()->page_action_controller()->Show(
      kActionContextualPanelPageActionChip);
}

ContextualTasksPageActionController::~ContextualTasksPageActionController() =
    default;

// static:
ContextualTasksPageActionController* ContextualTasksPageActionController::From(
    tabs::TabInterface* tab_interface) {
  return Get(tab_interface->GetUnownedUserDataHost());
}

void ContextualTasksPageActionController::InvokePageAction() {
  tab_interface_->GetBrowserWindowInterface()
      ->GetFeatures()
      .side_panel_ui()
      ->Toggle(SidePanelEntryKey(SidePanelEntryId::kContextualTasks),
               SidePanelOpenTrigger::kAppMenu);
}
