// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
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

  hide_popup_timer_.Stop();
  toast_timer_.Stop();
  loading_start_time_.reset();
  base::OnceClosure previous_cancel_callback =
      std::move(loading_cancel_callback_);
  if (prompt_state_ == PromptState::kShowing ||
      prompt_state_ == PromptState::kLoading) {
    HidePopupImpl();
  }
  if (previous_cancel_callback) {
    std::move(previous_cancel_callback).Run();
  }
  prompt_state_ = PromptState::kShowing;

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
    prompt_state_ = PromptState::kLoading;
    StartLoadingTimer();
  } else if (prompt_state_ == PromptState::kShowing) {
    prompt_state_ = PromptState::kClosed;
  }
}

void EmailVerificationController::HidePopup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (prompt_state_) {
    case PromptState::kClosed:
      return;
    case PromptState::kShowing:
      HidePopupImpl();
      prompt_state_ = PromptState::kClosed;
      return;
    case PromptState::kLoading: {
      loading_cancel_callback_.Reset();
      base::TimeDelta delay = GetRemainingLoadingDuration();
      if (!delay.is_zero()) {
        if (!hide_popup_timer_.IsRunning()) {
          hide_popup_timer_.Start(
              FROM_HERE, delay,
              base::BindOnce(&EmailVerificationController::HidePopup,
                             GetWeakPtr()));
        }
        return;
      }
      hide_popup_timer_.Stop();
      loading_start_time_.reset();
      HidePopupImpl();
      prompt_state_ = PromptState::kClosed;
      return;
    }
  }
}

void EmailVerificationController::ShowLoadingToast(
    base::OnceClosure cancel_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hide_popup_timer_.Stop();
  toast_timer_.Stop();
  loading_start_time_.reset();
  base::OnceClosure previous_cancel_callback =
      std::move(loading_cancel_callback_);
  if (prompt_state_ == PromptState::kShowing ||
      prompt_state_ == PromptState::kLoading) {
    HidePopupImpl();
    prompt_state_ = PromptState::kClosed;
  }
  if (previous_cancel_callback) {
    std::move(previous_cancel_callback).Run();
  }
  loading_cancel_callback_ = std::move(cancel_callback);
  StartLoadingTimer();
  ShowLoadingToastImpl(base::BindOnce(
      &EmailVerificationController::OnLoadingCancelled, GetWeakPtr()));
}

void EmailVerificationController::ShowErrorToast() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (DeferToast(base::BindOnce(
          &EmailVerificationController::ShowErrorToastImpl, GetWeakPtr()))) {
    return;
  }
  ShowErrorToastImpl();
}

void EmailVerificationController::ShowVerifiedToast(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (DeferToast(
          base::BindOnce(&EmailVerificationController::ShowVerifiedToastImpl,
                         GetWeakPtr(), url))) {
    return;
  }
  ShowVerifiedToastImpl(url);
}

bool EmailVerificationController::IsWebContentsVisible() const {
  return !web_contents_->IsBeingDestroyed() &&
         web_contents_->GetVisibility() != content::Visibility::HIDDEN;
}

bool EmailVerificationController::IsFirstRunRequestInFlight() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return prompt_state_ == PromptState::kLoading &&
         !hide_popup_timer_.IsRunning();
}

bool EmailVerificationController::IsLoadingToastActive() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return prompt_state_ == PromptState::kClosed &&
         loading_start_time_.has_value();
}

void EmailVerificationController::StartLoadingTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loading_start_time_ = base::TimeTicks::Now();
}

void EmailVerificationController::OnPromptDismissed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (prompt_state_ != PromptState::kLoading ||
      !loading_start_time_.has_value()) {
    return;
  }
  ResetLoadingStateAndFlushToast();
}

void EmailVerificationController::OnLoadingCancelled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsLoadingToastActive()) {
    return;
  }
  ResetLoadingStateAndFlushToast();
  if (loading_cancel_callback_) {
    std::move(loading_cancel_callback_).Run();
  }
}

void EmailVerificationController::ResetLoadingStateAndFlushToast() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  prompt_state_ = PromptState::kClosed;
  hide_popup_timer_.Stop();
  loading_start_time_.reset();
  if (toast_timer_.IsRunning()) {
    // If the verification response already arrived during the 800ms minimum
    // loading duration (so the token was already sent to the page), show the
    // deferred completion notification immediately if the tab is still visible,
    // or cancel it if the tab is hidden or being destroyed.
    if (IsWebContentsVisible()) {
      toast_timer_.FireNow();
    } else {
      toast_timer_.Stop();
    }
  }
}

bool EmailVerificationController::DeferToast(
    base::OnceClosure show_toast_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Once a completion notification (verified or error) is triggered, the
  // verification request has already finished and can no longer be cancelled.
  loading_cancel_callback_.Reset();
  base::TimeDelta delay = GetRemainingLoadingDuration();
  if (!delay.is_zero()) {
    toast_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&EmailVerificationController::OnToastTimerFired,
                       GetWeakPtr(), std::move(show_toast_callback)));
    return true;
  }
  toast_timer_.Stop();
  loading_start_time_.reset();
  return false;
}

void EmailVerificationController::OnToastTimerFired(
    base::OnceClosure show_toast_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loading_start_time_.reset();
  std::move(show_toast_callback).Run();
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
