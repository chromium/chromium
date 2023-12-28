// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_LAUNCH_APP_USER_CHOICE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_LAUNCH_APP_USER_CHOICE_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace views {
class Checkbox;
class ImageView;
class View;
}  // namespace views

namespace web_app {

// A free-floating dialog that presents the user with two choices (allow and
// deny), along with an option for persistence, wrt a particular web app. This
// dialog is shown before Chrome launches an app window for protocol handler or
// file handler launches.
class LaunchAppUserChoiceDialogView : public views::DialogDelegateView {
  METADATA_HEADER(LaunchAppUserChoiceDialogView, views::DialogDelegateView)

 public:
  LaunchAppUserChoiceDialogView(Profile* profile,
                                const webapps::AppId& app_id,
                                WebAppLaunchAcceptanceCallback close_callback);

  LaunchAppUserChoiceDialogView(const LaunchAppUserChoiceDialogView&) = delete;
  LaunchAppUserChoiceDialogView& operator=(
      const LaunchAppUserChoiceDialogView&) = delete;
  ~LaunchAppUserChoiceDialogView() override;

  void Init();

  static void SetDefaultRememberSelectionForTesting(bool remember_selection);

 protected:
  virtual std::unique_ptr<views::View> CreateAboveAppInfoView() = 0;
  virtual std::unique_ptr<views::View> CreateBelowAppInfoView() = 0;
  virtual std::u16string GetRememberChoiceString() = 0;

  Profile* profile() { return profile_; }
  const webapps::AppId& app_id() { return app_id_; }

 private:
  const webapps::AppId& GetSelectedAppId() const;
  void OnAccepted();
  void OnCanceled();
  void OnClosed();
  void InitChildViews();
  void OnIconsRead(std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  // Runs the close_callback_ provided during Show() if it exists.
  void RunCloseCallback(bool allowed, bool remember_user_choice);

  const raw_ptr<Profile> profile_;
  const webapps::AppId app_id_;
  raw_ptr<views::Checkbox> remember_selection_checkbox_;
  raw_ptr<views::ImageView> icon_image_view_;
  WebAppLaunchAcceptanceCallback close_callback_;
  base::WeakPtrFactory<LaunchAppUserChoiceDialogView> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_LAUNCH_APP_USER_CHOICE_DIALOG_VIEW_H_
