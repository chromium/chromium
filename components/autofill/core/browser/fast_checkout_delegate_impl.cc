// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/fast_checkout_delegate_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/logging/log_macros.h"

namespace autofill {

FastCheckoutDelegateImpl::FastCheckoutDelegateImpl(
    BrowserAutofillManager* manager)
    : manager_(manager) {
  DCHECK(manager);
}

FastCheckoutDelegateImpl::~FastCheckoutDelegateImpl() {
  // Invalidate pointers to avoid post hide callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  HideFastCheckoutUI();
}

bool FastCheckoutDelegateImpl::TryToShowFastCheckout(
    const FormData& form,
    const FormFieldData& field) {
#if BUILDFLAG(IS_ANDROID)
  LOG_AF(manager_->client()->GetLogManager())
      << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
      << "Start of trying to trigger Fast Checkout.";
#endif

  // Trigger only on supported platforms.
  if (!manager_->client()->IsFastCheckoutSupported()) {
    LOG_AF(manager_->client()->GetLogManager())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because it is not supported.";
    return false;
  }

  // Trigger only if the form is a trigger form for FC.
  if (!manager_->client()->IsFastCheckoutTriggerForm(form, field)) {
    LOG_AF(manager_->client()->GetLogManager())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because `form` is not a trigger form.";
    return false;
  }

  // UMA drop out metrics are recorded after this point only to avoid collecting
  // unnecessary metrics that would dominate the other data points.
  // Trigger only if not shown before.
  if (fast_checkout_state_ != FastCheckoutState::kNotShownYet) {
    base::UmaHistogramEnumeration(
        kUmaKeyFastCheckoutTriggerOutcome,
        FastCheckoutTriggerOutcome::kFailureShownBefore);
    LOG_AF(manager_->client()->GetLogManager())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because it was shown before.";
    return false;
  }

  // Trigger only on focusable fields.
  if (!field.is_focusable) {
    base::UmaHistogramEnumeration(
        kUmaKeyFastCheckoutTriggerOutcome,
        FastCheckoutTriggerOutcome::kFailureFieldNotFocusable);
    LOG_AF(manager_->client()->GetLogManager())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because field was not focusable.";
    return false;
  }

  // Trigger only on empty fields.
  if (!field.value.empty()) {
    base::UmaHistogramEnumeration(
        kUmaKeyFastCheckoutTriggerOutcome,
        FastCheckoutTriggerOutcome::kFailureFieldNotEmpty);
    LOG_AF(manager_->client()->GetLogManager())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because field was not empty.";
    return false;
  }

  // Trigger only if the UI is available.
  if (!manager_->driver()->CanShowAutofillUi()) {
    base::UmaHistogramEnumeration(
        kUmaKeyFastCheckoutTriggerOutcome,
        FastCheckoutTriggerOutcome::kFailureCannotShowAutofillUi);
    LOG_AF(manager_->client()->GetLogManager())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because Autofill UI cannot be shown.";
    return false;
  }

  // Finally try showing the surface.
  if (!manager_->client()->ShowFastCheckout(GetWeakPtr())) {
    LOG_AF(manager_->client()->GetLogManager())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "An error occurred while trying to show the Fast Checkout UI.";
    return false;
  }

  fast_checkout_state_ = FastCheckoutState::kIsShowing;
  manager_->client()->HideAutofillPopup(
      PopupHidingReason::kOverlappingWithFastCheckoutSurface);
  base::UmaHistogramEnumeration(kUmaKeyFastCheckoutTriggerOutcome,
                                FastCheckoutTriggerOutcome::kSuccess);
  LOG_AF(manager_->client()->GetLogManager())
      << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
      << "was triggered successfully.";
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
