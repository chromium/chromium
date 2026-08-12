// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_TEST_API_H_

#include "base/types/optional_util.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"
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

  base::WeakPtr<AutofillPopupView> view() { return controller_->view_; }

  AutofillPopupHideHelper* popup_hide_helper() {
    return base::OptionalToPtr(controller_->popup_hide_helper_);
  }

  // Determines whether to suppress minimum show thresholds. It should only be
  // set during tests that cannot mock time (e.g. the autofill interactive
  // browsertests).
  void DisableThreshold(bool disable_threshold) {
    controller_->disable_threshold_for_testing_ = disable_threshold;
  }

  void SetSuggestions(std::vector<Suggestion> suggestions) {
    controller_->SetSuggestions(std::move(suggestions));
  }

  void SetPreferPrevArrowSideOnSuggestionsUpdate(bool prefer_prev_arrow_side) {
    controller_->controller_common_
        .prefer_prev_arrow_side_on_suggestions_update = prefer_prev_arrow_side;
  }

  void SetShowTabbedPopup(bool show_tabbed_popup) {
    controller_->controller_common_.show_tabbed_popup = show_tabbed_popup;
  }

  void ClearState() { controller_->ClearState(); }

  bool HasEmptySuggestionContent() const {
    return controller_->HasEmptySuggestionContent();
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
