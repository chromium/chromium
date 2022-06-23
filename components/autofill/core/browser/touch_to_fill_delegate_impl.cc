// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/touch_to_fill_delegate_impl.h"

#include "components/autofill/core/browser/browser_autofill_manager.h"

namespace autofill {

TouchToFillDelegateImpl::TouchToFillDelegateImpl(
    BrowserAutofillManager* manager)
    : manager_(manager) {
  DCHECK(manager);
}

TouchToFillDelegateImpl::~TouchToFillDelegateImpl() = default;

bool TouchToFillDelegateImpl::TryToShowTouchToFill(int query_id,
                                                   const FormData& form,
                                                   const FormFieldData& field) {
  if (!manager_->client()->IsTouchToFillCreditCardSupported())
    return false;
  // TODO(crbug.com/1247698): Add additional eligibility checks.
  return manager_->client()->ShowTouchToFillCreditCard(GetWeakPtr());
}

void TouchToFillDelegateImpl::HideTouchToFill() {
  if (manager_->client()->IsTouchToFillCreditCardSupported()) {
    manager_->client()->HideTouchToFillCreditCard();
  }
}

base::WeakPtr<TouchToFillDelegateImpl> TouchToFillDelegateImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
