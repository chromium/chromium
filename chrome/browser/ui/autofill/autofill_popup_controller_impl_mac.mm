// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_mac.h"

#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"

using base::WeakPtr;

namespace autofill {

// static
WeakPtr<AutofillPopupControllerImpl> AutofillPopupControllerImpl::GetOrCreate(
    WeakPtr<AutofillPopupControllerImpl> previous,
    WeakPtr<AutofillPopupDelegate> delegate,
    content::WebContents* web_contents,
    gfx::NativeView container_view,
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction) {
  if (previous.get() && previous->delegate_.get() == delegate.get() &&
      previous->container_view() == container_view) {
    previous->SetElementBounds(element_bounds);
    previous->ClearState();
    return previous;
  }

  if (previous.get())
    previous->Hide(PopupHidingReason::kViewDestroyed);

  AutofillPopupControllerImpl* controller = new AutofillPopupControllerImplMac(
      delegate, web_contents, container_view, element_bounds, text_direction);
  return controller->GetWeakPtr();
}

AutofillPopupControllerImplMac::AutofillPopupControllerImplMac(
    base::WeakPtr<AutofillPopupDelegate> delegate,
    content::WebContents* web_contents,
    gfx::NativeView container_view,
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction)
    : AutofillPopupControllerImpl(delegate,
                                  web_contents,
                                  container_view,
                                  element_bounds,
                                  text_direction),
      touch_bar_controller_(nil),
      is_credit_card_popup_(delegate->GetPopupType() ==
                            PopupType::kCreditCards) {}

AutofillPopupControllerImplMac::~AutofillPopupControllerImplMac() {}

void AutofillPopupControllerImplMac::Show(
    std::vector<autofill::Suggestion> suggestions,
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  if (!suggestions.empty() && is_credit_card_popup_) {
    touch_bar_controller_ = [WebTextfieldTouchBarController
        controllerForWindow:[container_view().GetNativeNSView() window]];
    [touch_bar_controller_ showCreditCardAutofillWithController:this];
  }

  AutofillPopupControllerImpl::Show(std::move(suggestions),
                                    autoselect_first_suggestion);
  // No code below this line!
  // |Show| may hide the popup and destroy |this|, so |Show| should be the last
  // line.
}

void AutofillPopupControllerImplMac::UpdateDataListValues(
    const std::vector<std::u16string>& values,
    const std::vector<std::u16string>& labels) {
  if (touch_bar_controller_)
    [touch_bar_controller_ invalidateTouchBar];

  AutofillPopupControllerImpl::UpdateDataListValues(values, labels);
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
