// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
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
