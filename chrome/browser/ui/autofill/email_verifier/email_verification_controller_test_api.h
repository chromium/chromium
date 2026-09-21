// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_TEST_API_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"

namespace autofill {

class EmailVerificationControllerTestApi {
 public:
  explicit EmailVerificationControllerTestApi(
      EmailVerificationController& controller)
      : controller_(controller) {}

  EmailVerificationPopupController* popup_controller() {
    return controller_->popup_controller_.get();
  }
  void set_popup_controller(
      std::unique_ptr<EmailVerificationPopupController> controller) {
    controller_->popup_controller_ = std::move(controller);
  }

  bool is_hide_popup_timer_running() const {
    return controller_->hide_popup_timer_.IsRunning();
  }
  bool is_toast_timer_running() const {
    return controller_->toast_timer_.IsRunning();
  }
  std::optional<base::TimeTicks> loading_start_time() const {
    return controller_->loading_start_time_;
  }

 private:
  const raw_ref<EmailVerificationController> controller_;
};

inline EmailVerificationControllerTestApi test_api(
    EmailVerificationController& controller) {
  return EmailVerificationControllerTestApi(controller);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_TEST_API_H_
