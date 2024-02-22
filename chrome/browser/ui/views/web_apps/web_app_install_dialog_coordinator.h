// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_COORDINATOR_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "ui/views/view_tracker.h"

namespace views {
class View;
}  // namespace views

class Browser;

class PWAConfirmationBubbleView;

namespace web_app {

class WebAppInstallDialogCoordinator
    : public BrowserUserData<WebAppInstallDialogCoordinator> {
 public:
  ~WebAppInstallDialogCoordinator() override;

  bool IsShowing();
  PWAConfirmationBubbleView* GetBubbleView();
  void StartTracking(views::View* view);
  void StopTracking();

 private:
  explicit WebAppInstallDialogCoordinator(Browser* browser);
  friend BrowserUserData;

  views::ViewTracker install_dialog_tracker_;

  BROWSER_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_COORDINATOR_H_
