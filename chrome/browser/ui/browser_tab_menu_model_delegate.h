// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TAB_MENU_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_TAB_MENU_MODEL_DELEGATE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "components/sessions/core/session_id.h"

class Browser;
class Profile;

namespace web_app {
class AppBrowserController;
}

namespace chrome {

// Implementation of TabMenuModelDelegate which uses an instance of Browser to
// fulfill its duties.
class BrowserTabMenuModelDelegate : public TabMenuModelDelegate {
 public:
  BrowserTabMenuModelDelegate(
      SessionID session_id,
      const Profile* profile,
      const web_app::AppBrowserController* app_controller);
  ~BrowserTabMenuModelDelegate() override;

  BrowserTabMenuModelDelegate(const BrowserTabMenuModelDelegate&) = delete;
  BrowserTabMenuModelDelegate& operator=(const BrowserTabMenuModelDelegate&) =
      delete;

 private:
  // TabMenuModelDelegate:
  std::vector<Browser*> GetOtherBrowserWindows(bool is_app) override;

  const SessionID session_id_;
  const raw_ptr<const Profile> profile_;
  const raw_ptr<const web_app::AppBrowserController> app_controller_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TAB_MENU_MODEL_DELEGATE_H_
