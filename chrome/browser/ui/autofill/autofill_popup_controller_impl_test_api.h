// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_TEST_API_H_

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/next_idle_barrier.h"

namespace autofill {

// Exposes some testing operations for `AutofillPopupControllerImpl`.
class AutofillPopupControllerImplTestApi {
 public:
  explicit AutofillPopupControllerImplTestApi(
      AutofillPopupControllerImpl* controller)
      : controller_(*controller) {}

  void SetView(base::WeakPtr<AutofillPopupView> view) {
    controller_->view_ = std::move(view);
    controller_->barrier_for_accepting_ =
        NextIdleBarrier::CreateNextIdleBarrierWithDelay(
            AutofillSuggestionController::
                kIgnoreEarlyClicksOnSuggestionsDuration);
  }

  // Determines whether to suppress minimum show thresholds. It should only be
  // set during tests that cannot mock time (e.g. the autofill interactive
  // browsertests).
  void DisableThreshold(bool disable_threshold) {
    controller_->disable_threshold_for_testing_ = disable_threshold;
  }

 private:
  const raw_ref<AutofillPopupControllerImpl> controller_;
};

inline AutofillPopupControllerImplTestApi test_api(
    AutofillPopupControllerImpl& controller) {
  return AutofillPopupControllerImplTestApi(&controller);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_TEST_API_H_
