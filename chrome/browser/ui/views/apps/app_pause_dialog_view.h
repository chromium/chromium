// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_PAUSE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_PAUSE_DIALOG_VIEW_H_

#include "base/macros.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class ImageSkia;
}

// The app pause dialog. Once the user clicks the 'OK' button, this class calls
// the callback to notify AppService, which pauses the app.
class AppPauseDialogView : public views::BubbleDialogDelegateView {
 public:
  AppPauseDialogView(
      const std::string& app_name,
      gfx::ImageSkia image,
      const apps::PauseData& pause_data,
      apps::AppServiceProxy::OnPauseDialogClosedCallback callback);
  ~AppPauseDialogView() override;

  // views::BubbleDialogDelegateView:
  bool Accept() override;
  gfx::Size CalculatePreferredSize() const override;
  int GetDialogButtons() const override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

 private:
  // Callback when the dialog closes after the user has clicked the OK.
  apps::AppServiceProxy::OnPauseDialogClosedCallback closed_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_PAUSE_DIALOG_VIEW_H_
