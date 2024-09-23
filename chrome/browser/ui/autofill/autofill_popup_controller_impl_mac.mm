// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_mac.h"

#import <utility>

#import "chrome/browser/ui/autofill/popup_controller_common.h"
#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"

using base::WeakPtr;

namespace autofill {

// static
WeakPtr<AutofillSuggestionController> AutofillSuggestionController::GetOrCreate(
    WeakPtr<AutofillSuggestionController> previous,
    WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common,
    int32_t form_control_ax_id) {
  if (AutofillPopupControllerImpl* previous_impl =
          static_cast<AutofillPopupControllerImpl*>(previous.get());
      previous_impl && previous_impl->delegate_.get() == delegate.get() &&
      previous_impl->container_view() == controller_common.container_view) {
    previous_impl->controller_common_ = std::move(controller_common);
    previous_impl->form_control_ax_id_ = form_control_ax_id;
    previous_impl->ClearState();
    return previous_impl->GetWeakPtr();
  }

  if (previous.get())
    previous->Hide(SuggestionHidingReason::kViewDestroyed);

  auto* controller = new AutofillPopupControllerImplMac(
      delegate, web_contents, std::move(controller_common), form_control_ax_id);
  return controller->GetWeakPtr();
}

AutofillPopupControllerImplMac::AutofillPopupControllerImplMac(
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common,
    int32_t form_control_ax_id)
    : AutofillPopupControllerImpl(delegate,
                                  web_contents,
                                  std::move(controller_common),
                                  form_control_ax_id,
                                  std::nullopt),
      touch_bar_controller_(nil),
      is_credit_card_popup_(delegate->GetMainFillingProduct() ==
                            FillingProduct::kCreditCard) {}

AutofillPopupControllerImplMac::~AutofillPopupControllerImplMac() = default;

void AutofillPopupControllerImplMac::Show(
    UiSessionId ui_session_id,
    std::vector<autofill::Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  if (!suggestions.empty() && is_credit_card_popup_) {
    touch_bar_controller_ = [WebTextfieldTouchBarController
        controllerForWindow:[container_view().GetNativeNSView() window]];
    [touch_bar_controller_ showCreditCardAutofillWithController:this];
  }

  AutofillPopupControllerImpl::Show(ui_session_id, std::move(suggestions),
                                    trigger_source,
                                    autoselect_first_suggestion);
  // No code below this line!
  // |Show| may hide the popup and destroy |this|, so |Show| should be the last
  // line.
}

void AutofillPopupControllerImplMac::UpdateDataListValues(
    base::span<const SelectOption> options) {
  if (touch_bar_controller_)
    [touch_bar_controller_ invalidateTouchBar];

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
