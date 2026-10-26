// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

EmailVerificationController::EmailVerificationController(
    content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)) {}

EmailVerificationController::~EmailVerificationController() = default;

void EmailVerificationController::ShowPopup(
    const gfx::RectF& element_bounds,
    const url::Origin& issuer_origin,
    const std::u16string& email,
    base::OnceCallback<
        void(AutofillClient::EmailVerificationPermissionUiStatus)>
        decision_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsFirstRunRequestInFlight()) {
    // TODO(crbug.com/554034658): Consolidate metric reporting into a helper
    // method.
    base::UmaHistogramEnumeration(
        "Blink.Evp.PermissionUi.Status",
        AutofillClient::EmailVerificationPermissionUiStatus::
            kOverlappingPrompt);
    std::move(decision_callback)
        .Run(AutofillClient::EmailVerificationPermissionUiStatus::
                 kOverlappingPrompt);
    return;
  }

  DismissActiveUi();
  hide_active_ui_callback_ =
      base::BindOnce(&EmailVerificationController::HidePopupImpl, GetWeakPtr());
  ShowPopupImpl(
      element_bounds, issuer_origin, email,
      base::BindOnce(
          [](base::WeakPtr<EmailVerificationController> self,
             base::OnceCallback<void(
                 AutofillClient::EmailVerificationPermissionUiStatus)> callback,
             AutofillClient::EmailVerificationPermissionUiStatus status) {
            if (self) {
              DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
              self->OnPromptDecision(status);
            }
            std::move(callback).Run(status);
          },
          GetWeakPtr(), std::move(decision_callback)),
      base::BindOnce(&EmailVerificationController::OnPromptDismissed,
                     GetWeakPtr()));
}

void EmailVerificationController::OnPromptDecision(
    AutofillClient::EmailVerificationPermissionUiStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/554034658): Record the Blink.Evp.PermissionUi.Status
  // histogram here across Desktop and Android in a follow-up CL.
  if (status == AutofillClient::EmailVerificationPermissionUiStatus::kAllowed) {
    StartLoadingTimer();
  } else {
    hide_active_ui_callback_.Reset();
  }
}

void EmailVerificationController::HidePopup(CompletionStatus status,
                                            const GURL& verified_issuer_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Once verification finishes, the in-flight request can no longer be
  // cancelled by the user. Keep a no-op closure in `loading_cancel_callback_`
  // while `hide_popup_timer_` is pending so `IsLoadingToastActive()` remains
  // true until the loading notification is actually dismissed.
  if (loading_cancel_callback_) {
    loading_cancel_callback_ = base::DoNothing();
  }
  base::TimeDelta delay = GetRemainingLoadingDuration();
  if (!delay.is_zero()) {
    hide_popup_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&EmailVerificationController::HidePopup, GetWeakPtr(),
                       status, verified_issuer_url));
    return;
  }
  DismissActiveUi();
  switch (status) {
    case CompletionStatus::kAborted:
      break;
    case CompletionStatus::kError:
      ShowErrorToastImpl();
      break;
    case CompletionStatus::kVerified:
      ShowVerifiedToastImpl(verified_issuer_url);
      break;
  }
}

void EmailVerificationController::ShowLoadingToast(
    base::OnceClosure cancel_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DismissActiveUi();
  hide_active_ui_callback_ = base::BindOnce(
      &EmailVerificationController::HideLoadingToastImpl, GetWeakPtr());
  loading_cancel_callback_ =
      cancel_callback ? std::move(cancel_callback) : base::DoNothing();
  StartLoadingTimer();
  ShowLoadingToastImpl(base::BindOnce(
      &EmailVerificationController::OnLoadingCancelled, GetWeakPtr()));
}

bool EmailVerificationController::IsWebContentsVisible() const {
  return !web_contents_->IsBeingDestroyed() &&
         web_contents_->GetVisibility() != content::Visibility::HIDDEN;
}

void EmailVerificationController::DismissActiveUi() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hide_popup_timer_.Stop();
  loading_start_time_.reset();
  base::OnceClosure previous_cancel_callback =
      std::move(loading_cancel_callback_);
  if (hide_active_ui_callback_) {
    std::move(hide_active_ui_callback_).Run();
  }
  if (previous_cancel_callback) {
    std::move(previous_cancel_callback).Run();
  }
}

bool EmailVerificationController::IsFirstRunRequestInFlight() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return loading_start_time_.has_value() && !IsLoadingToastActive() &&
         !hide_popup_timer_.IsRunning();
}

bool EmailVerificationController::IsLoadingToastActive() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !loading_cancel_callback_.is_null();
}

void EmailVerificationController::StartLoadingTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loading_start_time_ = base::TimeTicks::Now();
}

void EmailVerificationController::OnPromptDismissed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!loading_start_time_.has_value() || IsLoadingToastActive() ||
      !hide_active_ui_callback_) {
    return;
  }
  ResetLoadingStateAndFlushToast();
}

void EmailVerificationController::OnLoadingCancelled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsLoadingToastActive() || !hide_active_ui_callback_) {
    return;
  }
  base::OnceClosure cancel_callback = std::move(loading_cancel_callback_);
  ResetLoadingStateAndFlushToast();
  std::move(cancel_callback).Run();
}

void EmailVerificationController::ResetLoadingStateAndFlushToast() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hide_active_ui_callback_.Reset();
  loading_cancel_callback_.Reset();
  loading_start_time_.reset();
  if (hide_popup_timer_.IsRunning()) {
    // If the verification response already arrived during the 800ms minimum
    // loading duration (so the token was already sent to the page), show the
    // deferred completion notification immediately if the tab is still visible,
    // or cancel it if the tab is hidden or being destroyed.
    if (IsWebContentsVisible()) {
      hide_popup_timer_.FireNow();
    } else {
      hide_popup_timer_.Stop();
    }
  }
}

base::TimeDelta EmailVerificationController::GetRemainingLoadingDuration()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If a loading UI is active (either the first-run in-button spinner or the
  // subsequent-run loading toast), enforce kMinimumLoadingDuration (800ms) to
  // avoid flickering when the network responds quickly (e.g. within 50-100ms).
  if (!loading_start_time_.has_value()) {
    return base::TimeDelta();
  }
  base::TimeDelta elapsed = base::TimeTicks::Now() - *loading_start_time_;
  if (elapsed < kMinimumLoadingDuration) {
    return kMinimumLoadingDuration - elapsed;
  }
  return base::TimeDelta();
}

}  // namespace autofill
