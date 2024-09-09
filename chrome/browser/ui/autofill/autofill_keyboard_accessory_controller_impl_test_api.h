// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_IMPL_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_IMPL_TEST_API_H_

#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_view.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/next_idle_barrier.h"

namespace autofill {

// Exposes some testing operations for
// `AutofillKeyboardAccessoryControllerImpl`.
class AutofillKeyboardAccessoryControllerImplTestApi {
 public:
  explicit AutofillKeyboardAccessoryControllerImplTestApi(
      AutofillKeyboardAccessoryControllerImpl* controller)
      : controller_(*controller) {}

  // Determines whether to suppress minimum show thresholds. It should only be
  // set during tests that cannot mock time (e.g. the autofill interactive
  // browsertests).
  void DisableThreshold(bool disable_threshold) {
    controller_->disable_threshold_for_testing_ = disable_threshold;
  }

  void SetView(std::unique_ptr<AutofillKeyboardAccessoryView> view) {
    controller_->view_ = std::move(view);
    controller_->barrier_for_accepting_ =
        NextIdleBarrier::CreateNextIdleBarrierWithDelay(
            AutofillSuggestionController::
                kIgnoreEarlyClicksOnSuggestionsDuration);
  }

  AutofillKeyboardAccessoryView* view() const {
    return controller_->view_.get();
  }

  void SetAccessLossWarningBridge(
      std::unique_ptr<PasswordAccessLossWarningBridge> bridge) {
    controller_->access_loss_warning_bridge_ = std::move(bridge);
  }

  PasswordAccessLossWarningBridge* access_loss_warning_bridge() {
    return controller_->access_loss_warning_bridge_.get();
  }

 private:
  const raw_ref<AutofillKeyboardAccessoryControllerImpl> controller_;
};

inline AutofillKeyboardAccessoryControllerImplTestApi test_api(
    AutofillKeyboardAccessoryControllerImpl& controller) {
  return AutofillKeyboardAccessoryControllerImplTestApi(&controller);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_IMPL_TEST_API_H_
