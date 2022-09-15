// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/fast_checkout_delegate_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"

namespace autofill {

FastCheckoutDelegateImpl::FastCheckoutDelegateImpl(
    BrowserAutofillManager* manager)
    : manager_(manager) {
  DCHECK(manager);
}

FastCheckoutDelegateImpl::~FastCheckoutDelegateImpl() = default;

bool FastCheckoutDelegateImpl::TryToShowFastCheckout(
    const FormData& form,
    const FormFieldData& field) {
  // Trigger only on supported platforms.
  if (!manager_->client()->IsFastCheckoutSupported())
    return false;

  // Trigger only if the form is a trigger form for FC.
  if (!manager_->client()->IsFastCheckoutTriggerForm(form, field))
    return false;

  // UMA drop out metrics are recorded after this point only to avoid collecting
  // unnecessary metrics that would dominate the other data points.
  // Trigger only if not shown before.
  if (fast_checkout_state_ != FastCheckoutState::kNotShownYet) {
    base::UmaHistogramEnumeration(
        kUmaKeyFastCheckoutTriggerOutcome,
        FastCheckoutTriggerOutcome::kFailureShownBefore);
    return false;
  }

  // Trigger only on focusable fields.
  if (!field.is_focusable) {
    base::UmaHistogramEnumeration(
        kUmaKeyFastCheckoutTriggerOutcome,
        FastCheckoutTriggerOutcome::kFailureFieldNotFocusable);
    return false;
  }

  // Trigger only on empty fields.
  if (!field.value.empty()) {
    base::UmaHistogramEnumeration(
        kUmaKeyFastCheckoutTriggerOutcome,
        FastCheckoutTriggerOutcome::kFailureFieldNotEmpty);
    return false;
  }

  // Trigger only if the UI is available.
  if (!manager_->driver()->CanShowAutofillUi()) {
    base::UmaHistogramEnumeration(
        kUmaKeyFastCheckoutTriggerOutcome,
        FastCheckoutTriggerOutcome::kFailureCannotShowAutofillUi);
    return false;
  }

  // Finally try showing the surface.
  if (!manager_->client()->ShowFastCheckout(GetWeakPtr()))
    return false;

  fast_checkout_state_ = FastCheckoutState::kIsShowing;
  manager_->client()->HideAutofillPopup(
      PopupHidingReason::kOverlappingWithFastCheckoutSurface);
  base::UmaHistogramEnumeration(kUmaKeyFastCheckoutTriggerOutcome,
                                FastCheckoutTriggerOutcome::kSuccess);
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

AutofillDriver* FastCheckoutDelegateImpl::GetDriver() {
  return manager_->driver();
}

void FastCheckoutDelegateImpl::Reset() {
  HideFastCheckoutUI();
  fast_checkout_state_ = FastCheckoutState::kNotShownYet;
}

base::WeakPtr<FastCheckoutDelegateImpl> FastCheckoutDelegateImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
