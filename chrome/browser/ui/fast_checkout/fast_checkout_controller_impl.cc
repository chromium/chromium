// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fast_checkout/fast_checkout_controller_impl.h"

#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/browser/web_contents.h"

FastCheckoutControllerImpl::FastCheckoutControllerImpl(
    content::WebContents* web_contents,
    Delegate* delegate)
    : web_contents_(web_contents), delegate_(delegate) {}

FastCheckoutControllerImpl::~FastCheckoutControllerImpl() = default;

void FastCheckoutControllerImpl::Show() {}

void FastCheckoutControllerImpl::OnOptionsSelected(
    std::unique_ptr<autofill::AutofillProfile> profile,
    std::unique_ptr<autofill::CreditCard> credit_card) {
  delegate_->OnOptionsSelected(std::move(profile), std::move(credit_card));
}

void FastCheckoutControllerImpl::OnDismiss() {
  delegate_->OnDismiss();
}

gfx::NativeView FastCheckoutControllerImpl::GetNativeView() {
  return web_contents_->GetNativeView();
}
