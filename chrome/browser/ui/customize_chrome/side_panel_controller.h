// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

enum class CustomizeChromeSection;

namespace customize_chrome {

// Interface for interacting with and getting info for the sidepanel for
// CustomizeChrome SidePanel. Features should use this class and not
// SidePanelControllerViews unless they need direct access to creating the View
// component for the SidePanel.
class SidePanelController : public SidePanelEntryObserver {
 public:
  DECLARE_USER_DATA(SidePanelController);

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

  // Opens the Customize Chrome side panel. Implementations that support
  // sections use `section` to select one. `trigger` records how it was opened.
  virtual void OpenSidePanel(SidePanelOpenTrigger trigger,
                             std::optional<CustomizeChromeSection> section) = 0;

  // Closes the SidePanel if it's open to the CustomizeChrome page.
  virtual void CloseSidePanel() = 0;
};

}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
