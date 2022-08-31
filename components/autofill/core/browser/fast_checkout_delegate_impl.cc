// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/fast_checkout_delegate_impl.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"

namespace autofill {

FastCheckoutDelegateImpl::FastCheckoutDelegateImpl(
    BrowserAutofillManager* manager)
    : manager_(manager) {
  DCHECK(manager);
}

FastCheckoutDelegateImpl::~FastCheckoutDelegateImpl() = default;

// TODO(crbug.com/1334642): Add metrics to record reasons why FC is not shown.
bool FastCheckoutDelegateImpl::TryToShowFastCheckout(
    const FormData& form,
    const FormFieldData& field) {
  // Trigger only on supported platforms.
  if (!manager_->client()->IsFastCheckoutSupported())
    return false;
  // Trigger only if the form is a trigger form for FC.
  if (!manager_->client()->IsFastCheckoutTriggerForm(form, field))
    return false;
  // Trigger only if not shown before.
  if (fast_checkout_state_ != FastCheckoutState::kNotShownYet)
    return false;
  // Trigger only on focusable empty fields.
  if (!field.is_focusable || !field.value.empty())
    return false;
  // Trigger only if the UI is available.
  if (!manager_->driver()->CanShowAutofillUi())
    return false;

  // Finally try showing the surface.
  if (!manager_->client()->ShowFastCheckout(GetWeakPtr()))
    return false;

  fast_checkout_state_ = FastCheckoutState::kIsShowing;
  manager_->client()->HideAutofillPopup(
      PopupHidingReason::kOverlappingWithFastCheckoutSurface);
  return true;
}

bool FastCheckoutDelegateImpl::IsShowingFastCheckoutUI() const {
  return fast_checkout_state_ == FastCheckoutState::kIsShowing;
}

void FastCheckoutDelegateImpl::OnFastCheckoutUIHidden() {
  fast_checkout_state_ = FastCheckoutState::kWasShown;
}

// TODO(crbug.com/1348538): Create a central point for TTF/FC hiding decision.
void FastCheckoutDelegateImpl::HideFastCheckoutUI() {
  if (IsShowingFastCheckoutUI()) {
    manager_->client()->HideFastCheckout();
    fast_checkout_state_ = FastCheckoutState::kWasShown;
  }
}

void FastCheckoutDelegateImpl::Reset() {
  HideFastCheckoutUI();
  fast_checkout_state_ = FastCheckoutState::kNotShownYet;
}

base::WeakPtr<FastCheckoutDelegateImpl> FastCheckoutDelegateImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
