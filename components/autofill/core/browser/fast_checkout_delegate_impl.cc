// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/fast_checkout_delegate_impl.h"

namespace autofill {

FastCheckoutDelegateImpl::FastCheckoutDelegateImpl(
    BrowserAutofillManager* manager)
    : manager_(manager) {
  DCHECK(manager);
}

FastCheckoutDelegateImpl::~FastCheckoutDelegateImpl() = default;

bool FastCheckoutDelegateImpl::TryToShowFastCheckout(
    const FormFieldData& field) {
  // TODO(crbug.com/1334642): Implement.
  return false;
}

bool FastCheckoutDelegateImpl::IsShowingFastCheckoutUI() const {
  return fast_checkout_state_ == FastCheckoutState::kIsShowing;
}

void FastCheckoutDelegateImpl::OnFastCheckoutUIHidden() {
  fast_checkout_state_ = FastCheckoutState::kWasShown;
}

void FastCheckoutDelegateImpl::HideFastCheckoutUI() {
  // TODO(crbug.com/1334642): Implement.
}

void FastCheckoutDelegateImpl::Reset() {
  // TODO(crbug.com/1334642): Implement.
}

base::WeakPtr<FastCheckoutDelegateImpl> FastCheckoutDelegateImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
