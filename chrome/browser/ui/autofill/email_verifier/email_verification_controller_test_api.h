// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_TEST_API_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/autofill/email_verification_bottom_sheet_bridge.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller_android.h"
#else
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller_desktop.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"
#endif

namespace autofill {

// Exposes internal state of `EmailVerificationController` (and its platform
// subclasses) for unit tests.
class EmailVerificationControllerTestApi {
 public:
  explicit EmailVerificationControllerTestApi(
      EmailVerificationController& controller)
      : controller_(controller) {}

#if BUILDFLAG(IS_ANDROID)
  EmailVerificationBottomSheetBridge* bottom_sheet_bridge() {
    return static_cast<EmailVerificationControllerAndroid&>(*controller_)
        .bottom_sheet_bridge_.get();
  }
  void set_bottom_sheet_bridge(
      std::unique_ptr<EmailVerificationBottomSheetBridge> bridge) {
    CHECK(bridge);
    static_cast<EmailVerificationControllerAndroid&>(*controller_)
        .bottom_sheet_bridge_ = std::move(bridge);
  }
  void set_message_controller(AutofillMessageController* message_controller) {
    static_cast<EmailVerificationControllerAndroid&>(*controller_)
        .message_controller_for_testing_ = message_controller;
  }
  void set_open_settings_callback(base::RepeatingClosure callback) {
    static_cast<EmailVerificationControllerAndroid&>(*controller_)
        .open_settings_callback_ = std::move(callback);
  }
#else
  EmailVerificationPopupController& popup_controller() {
    return static_cast<EmailVerificationControllerDesktop&>(*controller_)
        .popup_controller_;
  }
#endif

  bool is_hide_popup_timer_running() const {
    return controller_->hide_popup_timer_.IsRunning();
  }
  std::optional<base::TimeTicks> loading_start_time() const {
    return controller_->loading_start_time_;
  }
  void OnLoadingCancelled() { controller_->OnLoadingCancelled(); }

 private:
  const raw_ref<EmailVerificationController> controller_;
};

inline EmailVerificationControllerTestApi test_api(
    EmailVerificationController& controller) {
  return EmailVerificationControllerTestApi(controller);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_TEST_API_H_
