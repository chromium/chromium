// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fast_checkout/fast_checkout_controller_impl.h"

#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/browser/web_contents.h"

FastCheckoutControllerImpl::FastCheckoutControllerImpl(
    content::WebContents* web_contents,
    Delegate* delegate)
    : web_contents_(web_contents), delegate_(delegate) {}

FastCheckoutControllerImpl::~FastCheckoutControllerImpl() = default;

void FastCheckoutControllerImpl::Show(
    const std::vector<const autofill::AutofillProfile*>& autofill_profiles,
    const std::vector<autofill::CreditCard*>& credit_cards) {
  GetOrCreateView()->Show(autofill_profiles, credit_cards);
}

void FastCheckoutControllerImpl::OnOptionsSelected(
    std::unique_ptr<autofill::AutofillProfile> profile,
    std::unique_ptr<autofill::CreditCard> credit_card) {
  view_.reset();
  delegate_->OnOptionsSelected(std::move(profile), std::move(credit_card));
}

void FastCheckoutControllerImpl::OnDismiss() {
  view_.reset();
  delegate_->OnDismiss();
}

FastCheckoutView* FastCheckoutControllerImpl::GetOrCreateView() {
  if (!view_) {
    view_ = FastCheckoutView::Create(weak_ptr_factory_.GetWeakPtr());
  }
  return view_.get();
}

void FastCheckoutControllerImpl::OpenAutofillProfileSettings() {
  autofill::ShowAutofillProfileSettings(web_contents_);
}

void FastCheckoutControllerImpl::OpenCreditCardSettings() {
  autofill::ShowAutofillCreditCardSettings(web_contents_);
}

gfx::NativeView FastCheckoutControllerImpl::GetNativeView() {
  return web_contents_->GetNativeView();
}
