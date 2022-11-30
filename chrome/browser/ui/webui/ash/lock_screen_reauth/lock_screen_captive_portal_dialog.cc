// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_captive_portal_dialog.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "components/captive_portal/core/captive_portal_detector.h"

namespace ash {

LockScreenCaptivePortalDialog::LockScreenCaptivePortalDialog()
    : BaseLockDialog(
          GURL(captive_portal::CaptivePortalDetector::kDefaultURL),
          LockScreenStartReauthDialog::CalculateLockScreenReauthDialogSize(
              features::IsNewLockScreenReauthLayoutEnabled())) {}

LockScreenCaptivePortalDialog::~LockScreenCaptivePortalDialog() = default;

void LockScreenCaptivePortalDialog::Show(Profile& profile) {
  DCHECK(ProfileHelper::IsLockScreenProfile(&profile));
  if (!is_running_) {
    ShowSystemDialogForBrowserContext(&profile);
    is_running_ = true;
  }
  if (on_shown_callback_for_testing_) {
    std::move(on_shown_callback_for_testing_).Run();
  }
}

void LockScreenCaptivePortalDialog::Dismiss() {
  if (is_running_) {
    // Order matters here because the value of `is_running_` affects behaviour
    // of `LockScreenCaptivePortalDialog::OnDialogClosed`
    is_running_ = false;
    Close();
  }
}

void LockScreenCaptivePortalDialog::OnDialogClosed(const std::string&) {
  // There are two ways in which captive portal dialog can be closed:
  // 1) Through `LockScreenCaptivePortalDialog::Dismiss()` called by
  // `LockScreenStartReauthDialog` when network goes online.
  // 2) Manually by user if they press esc or click x button in which case
  // `LockScreenCaptivePortalDialog::Dismiss()` won't be called.

  // In the later case we want to disconnect from current network to prevent
  // `LockScreenStartReauthDialog::UpdateState` from immediately reopening
  // captive portal dialog (since the network would still be in a captive portal
  // state).
  // We can distinguish these cases here by the value of `is_running_` because
  // `LockScreenCaptivePortalDialog::Dismiss()` sets it to false.
  if (is_running_) {
    const std::string network_path = NetworkHandler::Get()
                                         ->network_state_handler()
                                         ->DefaultNetwork()
                                         ->path();
    NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
        network_path, base::DoNothing(), network_handler::ErrorCallback());
    is_running_ = false;
  }
  if (on_closed_callback_for_testing_) {
    std::move(on_closed_callback_for_testing_).Run();
  }
}

bool LockScreenCaptivePortalDialog::IsRunning() const {
  return is_running_;
}

bool LockScreenCaptivePortalDialog::IsDialogClosedForTesting(
    base::OnceClosure callback) {
  if (!is_running_)
    return true;
  DCHECK(!on_closed_callback_for_testing_);
  on_closed_callback_for_testing_ = std::move(callback);
  return false;
}

bool LockScreenCaptivePortalDialog::IsDialogShownForTesting(
    base::OnceClosure callback) {
  if (is_running_)
    return true;
  DCHECK(!on_shown_callback_for_testing_);
  on_shown_callback_for_testing_ = std::move(callback);
  return false;
}

void LockScreenCaptivePortalDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::TYPE_WINDOW;
}

}  // namespace ash
