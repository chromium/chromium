// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_COORDINATOR_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget_observer.h"

class Browser;

namespace web_app {

class WebAppInstallDialogCoordinator
    : public BrowserUserData<WebAppInstallDialogCoordinator>,
      public views::WidgetObserver {
 public:
  ~WebAppInstallDialogCoordinator() override;

  bool IsShowing();
  views::BubbleDialogDelegate* GetBubbleView();
  void StartTracking(views::BubbleDialogDelegate* view);

  // WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  explicit WebAppInstallDialogCoordinator(Browser* browser);
  friend BrowserUserData;

  void StopTracking();
  void MaybeUpdatePwaAnchorViewIfNeeded();

  raw_ptr<views::BubbleDialogDelegate> dialog_delegate_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  BROWSER_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_COORDINATOR_H_
