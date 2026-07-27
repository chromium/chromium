// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_page_action_controller.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace autofill {

DEFINE_USER_DATA(OmniboxAutofillPageActionController);

OmniboxAutofillPageActionController::OmniboxAutofillPageActionController(
    tabs::TabInterface& tab_interface,
    page_actions::PageActionController& page_action_controller)
    : page_actions::PageActionObserver(kActionAutofillPayment),
      tab_interface_(tab_interface),
      page_action_controller_(page_action_controller),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  RegisterAsPageActionObserver(*page_action_controller_);
}

OmniboxAutofillPageActionController::~OmniboxAutofillPageActionController() =
    default;

// static
OmniboxAutofillPageActionController* OmniboxAutofillPageActionController::From(
    tabs::TabInterface& tab) {
  return Get(tab.GetUnownedUserDataHost());
}

void OmniboxAutofillPageActionController::OnPageActionChipShown(
    const page_actions::PageActionState& page_action) {
  if (BrowserWindowInterface* browser_window =
          tab_interface_->GetBrowserWindowInterface()) {
    if (BrowserUserEducationInterface* user_education =
            BrowserUserEducationInterface::From(browser_window)) {
      user_education->MaybeShowFeaturePromo(
          feature_engagement::kIPHAutofillOmniboxPaymentChipFeature);
    }
  }
  // Run `on_chip_shown_` at most once per `ShowExpandedChip()` request.
  if (on_chip_shown_) {
    std::move(on_chip_shown_).Run();
  }
}

void OmniboxAutofillPageActionController::ShowExpandedChip(
    base::OnceClosure on_chip_shown) {
  on_chip_shown_ = std::move(on_chip_shown);
  page_action_controller_->Show(kActionAutofillPayment);
  page_action_controller_->ShowSuggestionChip(kActionAutofillPayment,
                                              {.should_animate = true});
}

void OmniboxAutofillPageActionController::ShowCollapsedChip() {
  on_chip_shown_.Reset();
  page_action_controller_->Show(kActionAutofillPayment);
  page_action_controller_->HideSuggestionChip(kActionAutofillPayment);
}

void OmniboxAutofillPageActionController::HideChip() {
  on_chip_shown_.Reset();
  page_action_controller_->HideSuggestionChip(kActionAutofillPayment);
  page_action_controller_->Hide(kActionAutofillPayment);
}

}  // namespace autofill
