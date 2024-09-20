// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/task/delay_policy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"

namespace customize_chrome {

// Interface for interacting with and getting info for the sidepanel for
// CustomizeChrome SidePanel. Features should use this class and not
// SidePanelControllerViews unless they need direct access to creating the View
// component for the SidePanel.
//
// This class an abstract-base-class that serves no purpose other than to
// satisfy a historical constraint (no referencing views from outside of views)
// which has since been deleted.
// TODO(https://crbug.com/365591184) Clean up this abstraction by deleting this
// class altogether.
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

  // Calls the necessary code to show the sidepanel if needed, and then scrolls
  // the the |section| if its set. |trigger| is used for logging.
  virtual void OpenSidePanel(SidePanelOpenTrigger trigger,
                             std::optional<CustomizeChromeSection> section) = 0;

  // Closes the SidePanel if it's open to the CustomizeChrome page.
  virtual void CloseSidePanel() = 0;

  // Test helpers.
  // TODO (https://crbug.com/353343817): Remove these from tests and here.
  void CreateAndRegisterEntryForTesting() { CreateAndRegisterEntry(); }
  void DeregisterEntryForTesting() { DeregisterEntry(); }

 protected:
  // Creates the entry in the SidePanelRegistry for the tab if possible. If the
  // entry already exists then does nothing.
  virtual void CreateAndRegisterEntry() = 0;

  // Deletes the entry in the SidePanelRegistry for the tab if possible. If the
  // entry doesnt exist then does nothing.
  virtual void DeregisterEntry() = 0;
};

}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
