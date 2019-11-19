// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_mac.h"

#include "base/mac/availability.h"
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
    previous->Hide();

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
    const std::vector<autofill::Suggestion>& suggestions,
    bool autoselect_first_suggestion,
    PopupType popup_type) {
  if (!suggestions.empty() && is_credit_card_popup_) {
    if (@available(macOS 10.12.2, *)) {
      touch_bar_controller_ = [WebTextfieldTouchBarController
          controllerForWindow:[container_view().GetNativeNSView() window]];
      [touch_bar_controller_ showCreditCardAutofillWithController:this];
    }
  }

  AutofillPopupControllerImpl::Show(suggestions, autoselect_first_suggestion,
                                    popup_type);
  // No code below this line!
  // |Show| may hide the popup and destroy |this|, so |Show| should be the last
  // line.
}

void AutofillPopupControllerImplMac::UpdateDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  if (touch_bar_controller_)
    [touch_bar_controller_ invalidateTouchBar];

  AutofillPopupControllerImpl::UpdateDataListValues(values, labels);
  // No code below this line!
  // |UpdateDataListValues| may hide the popup and destroy |this|, so
  // |UpdateDataListValues| should be the last line.
}

void AutofillPopupControllerImplMac::Hide() {
  if (touch_bar_controller_) {
    [touch_bar_controller_ hideCreditCardAutofillTouchBar];
    touch_bar_controller_ = nil;
  }

  AutofillPopupControllerImpl::Hide();
  // No code below this line!
  // |Hide()| destroys |this|, so it should be the last line.
}

}  // namespace autofill
