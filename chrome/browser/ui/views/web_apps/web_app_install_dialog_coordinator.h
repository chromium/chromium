// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;

namespace web_app {

// TODO(b/326418546): Tracking using a weak_ptr and a raw_ptr to the bubble
// dialog delegate may lead to complicated ownership issues. Move to a
// WidgetObserver class, or store a weakptr to a BubbleDialogDelegate instead
// (needs views/ code modification and some scoping).
class WebAppInstallDialogCoordinator
    : public BrowserUserData<WebAppInstallDialogCoordinator> {
 public:
  ~WebAppInstallDialogCoordinator() override;

  bool IsShowing();
  views::BubbleDialogDelegate* GetBubbleView();
  void StartTracking(views::BubbleDialogDelegate* view);
  void StopTracking();
  base::WeakPtr<WebAppInstallDialogCoordinator> AsWeakPtr();

 private:
  explicit WebAppInstallDialogCoordinator(Browser* browser);
  friend BrowserUserData;

  raw_ptr<views::BubbleDialogDelegate> dialog_delegate_ = nullptr;

  base::WeakPtrFactory<WebAppInstallDialogCoordinator> weak_ptr_factory_{this};

  BROWSER_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_COORDINATOR_H_
