// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_DISCONNECT_FILE_CHOOSER_ON_BACKGROUND_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_DISCONNECT_FILE_CHOOSER_ON_BACKGROUND_CONTROLLER_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"

namespace tabs {

// File select dialogs generally do not have any indicator for which tab
// requested it, which can cause user confusion if the active tab changes for
// some reason. As a small and quick fix, disconnect the listener when the tab
// deactivates.
class DisconnectFileChooserOnBackgroundController {
 public:
  explicit DisconnectFileChooserOnBackgroundController(TabInterface& tab);
  ~DisconnectFileChooserOnBackgroundController();

 private:
  void WillEnterBackground(TabInterface* tab);

  base::CallbackListSubscription will_enter_background_subscription_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_DISCONNECT_FILE_CHOOSER_ON_BACKGROUND_CONTROLLER_H_
