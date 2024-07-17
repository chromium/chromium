// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"

namespace customize_chrome {

// Interface for interacting with and getting info for the sidepanel for
// CustomizeChrome SidePanel. Features should use this class and not
// SidePanelControllerViews unless they need direct access to creating the View
// component for the SidePanel.
class SidePanelController : public SidePanelEntryObserver {
 public:
  using StateChangedCallBack = base::RepeatingCallback<void(bool)>;

  ~SidePanelController() override = default;

  // Returns true if the sidepanel has registered customize chrome.
  virtual bool IsCustomizeChromeEntryAvailable() const = 0;

  // Returns true if the sidepanel is currently showing customize chrome.
  virtual bool IsCustomizeChromeEntryShowing() const = 0;

  // Sets a callback that will be called when the SidePanelEntryObserver
  // detects a change (OnEntryShown/OnEntryHidden). If there is already a
  // callback, just replaces it.
  virtual void SetEntryChangedCallback(StateChangedCallBack callback) = 0;

  // Attempts to change the state of the SidePanelUI to |visible|. Also attempts
  // to scroll to a specific section in the SidePanel. If it cant perform these
  // actions, silently returns.
  virtual void SetCustomizeChromeSidePanelVisible(
      bool visible,
      CustomizeChromeSection section) = 0;

  // Creates the entry in the SidePanelRegistry for the tab if possible. If the
  // entry already exists then does nothing.
  virtual void CreateAndRegisterEntry() = 0;

  // Deletes the entry in the SidePanelRegistry for the tab if possible. If the
  // entry doesnt exist then does nothing.
  virtual void DeregisterEntry() = 0;
};

}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
