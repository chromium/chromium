// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_SCRIM_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_SCRIM_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "components/tabs/public/tab_interface.h"

class BrowserWindowInterface;

namespace split_tabs {
class SplitTabScrimDelegate;

// Coordinates the split tab scrim to show and hide.
class SplitTabScrimController : public OmniboxTabHelper::Observer {
 public:
  SplitTabScrimController(std::unique_ptr<SplitTabScrimDelegate> delegate,
                          BrowserWindowInterface* browser_window);
  ~SplitTabScrimController() override;

  // OmniboxTabHelper::Observer:
  void OnOmniboxFocusChanged(OmniboxFocusState state,
                             OmniboxFocusChangeReason reason) override;
  void OnOmniboxInputStateChanged() override {}
  void OnOmniboxInputInProgress(bool in_progress) override {}
  void OnOmniboxPopupVisibilityChanged(bool popup_is_open) override {}

 private:
  void OnActiveTabChange(BrowserWindowInterface* browser_window_interface);
  void OnTabWillDetach(tabs::TabInterface* tab_interface,
                       tabs::TabInterface::DetachReason reason);
  void UpdateScrimVisibility();

  base::CallbackListSubscription active_tab_change_subscription_;
  base::CallbackListSubscription tab_will_detach_subscription_;
  base::ScopedObservation<OmniboxTabHelper, OmniboxTabHelper::Observer>
      omnibox_tab_helper_observation_{this};
  std::unique_ptr<SplitTabScrimDelegate> split_tab_scrim_delegate_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
};
}  // namespace split_tabs

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_SCRIM_CONTROLLER_H_
