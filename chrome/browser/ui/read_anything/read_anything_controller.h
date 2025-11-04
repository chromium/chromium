// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

// Manages the core logic for the Reading Mode feature.
//
// This controller is owned by TabFeatures and is instantiated once per tab.
// Its lifetime is tied to the TabInterface.
//
// It acts as the primary entry point for all Reading Mode commands and is
// responsible for orchestrating the display of the Reading Mode UI.
class ReadAnythingController {
 public:
  ReadAnythingController(const ReadAnythingController&) = delete;
  ReadAnythingController& operator=(const ReadAnythingController&) = delete;
  ~ReadAnythingController();

  explicit ReadAnythingController(tabs::TabInterface* tab);

  DECLARE_USER_DATA(ReadAnythingController);
  static ReadAnythingController* From(tabs::TabInterface* tab);

  // Displays the Reading Mode UI by utilizing the SidePanelUI on the active
  // tab.
  // TODO(crbug.com/447418049): Open immersive reading mode via this entrypoint.
  void ShowUI(SidePanelOpenTrigger trigger);

  // Toggles the Reading Mode UI by utilizing the SidePanelUI on the active
  // tab.
  // TODO(crbug.com/447418049): Open immersive reading mode via this entrypoint.
  void ToggleReadAnythingSidePanel(SidePanelOpenTrigger trigger);

 private:
  // Returns the SidePanelUI for the active tab if it can be shown.
  // Otherwise, returns nullptr.
  SidePanelUI* GetSidePanelUI();

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  ui::ScopedUnownedUserData<ReadAnythingController> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
