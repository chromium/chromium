// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_mac.h"

#import <utility>

#import "chrome/browser/ui/autofill/popup_controller_common.h"
#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"

using base::WeakPtr;

namespace autofill {

base::WeakPtr<AutofillSuggestionController>
CreateAutofillPopupControllerImplMac(
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common) {
  return (new AutofillPopupControllerImplMac(delegate, web_contents,
                                             std::move(controller_common)))
      ->GetWeakPtr();
}

AutofillPopupControllerImplMac::AutofillPopupControllerImplMac(
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common)
    : AutofillPopupControllerImpl(delegate,
                                  web_contents,
                                  std::move(controller_common),
                                  std::nullopt),
      touch_bar_controller_(nil) {}

AutofillPopupControllerImplMac::~AutofillPopupControllerImplMac() = default;

void AutofillPopupControllerImplMac::Show(
    UiSessionId ui_session_id,
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    AutoselectFirstSuggestion autoselect_first_suggestion,
    AutofillSuggestionsIgnoreFocusLoss ignore_focus_loss) {
  if (!suggestions.empty() && HasCreditCardSuggestions()) {
    touch_bar_controller_ = [WebTextfieldTouchBarController
        controllerForWindow:[container_view().GetNativeNSView() window]];
    [touch_bar_controller_ showCreditCardAutofillWithController:this];
  } else if (touch_bar_controller_) {
    [touch_bar_controller_ hideCreditCardAutofillTouchBar];
    touch_bar_controller_ = nil;
  }

  AutofillPopupControllerImpl::Show(ui_session_id, std::move(suggestions),
                                    trigger_source, autoselect_first_suggestion,
                                    ignore_focus_loss);
  // No code below this line!
  // |Show| may hide the popup and destroy |this|, so |Show| should be the last
  // line.
}

void AutofillPopupControllerImplMac::UpdateDataListValues(
    base::span<const SelectOption> options) {
  if (touch_bar_controller_) {
    [touch_bar_controller_ invalidateTouchBar];
  }

  AutofillPopupControllerImpl::UpdateDataListValues(options);
  // No code below this line!
  // |UpdateDataListValues| may hide the popup and destroy |this|, so
  // |UpdateDataListValues| should be the last line.
}

void AutofillPopupControllerImplMac::HideViewAndDie() {
  if (touch_bar_controller_) {
    [touch_bar_controller_ hideCreditCardAutofillTouchBar];
    touch_bar_controller_ = nil;
  }

  AutofillPopupControllerImpl::HideViewAndDie();
  // No code below this line!
  // |HideViewAndDie()| destroys |this|, so it should be the last line.
}

}  // namespace autofill
