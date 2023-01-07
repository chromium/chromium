// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_CAPTIVE_PORTAL_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_CAPTIVE_PORTAL_DIALOG_H_

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/base_lock_dialog.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace ash {

class LockScreenCaptivePortalDialog : public BaseLockDialog {
 public:
  LockScreenCaptivePortalDialog();
  LockScreenCaptivePortalDialog(LockScreenCaptivePortalDialog const&) = delete;
  ~LockScreenCaptivePortalDialog() override;

  void Show(Profile& profile);
  void Dismiss();
  void OnDialogClosed(const std::string& json_retval) override;
  bool IsRunning() const;

  // Used for waiting for the dialog to be closed or shown in tests.
  bool IsDialogClosedForTesting(base::OnceClosure callback);
  bool IsDialogShownForTesting(base::OnceClosure callback);

 private:
  bool is_running_ = false;

  base::OnceClosure on_closed_callback_for_testing_;
  base::OnceClosure on_shown_callback_for_testing_;

  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_CAPTIVE_PORTAL_DIALOG_H_
