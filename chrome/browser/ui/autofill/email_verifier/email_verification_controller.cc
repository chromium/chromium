// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verified_toast_menu_model.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

EmailVerificationController::EmailVerificationController(
    content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)),
      popup_controller_(web_contents) {}

EmailVerificationController::~EmailVerificationController() = default;

void EmailVerificationController::ShowPopup(
    const gfx::RectF& element_bounds_in_screen_space,
    const net::SchemefulSite& issuer_site,
    const std::u16string& email,
    base::OnceCallback<
        void(AutofillClient::EmailVerificationPermissionUiStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsFirstRunRequestInFlight()) {
    DismissPopup();
    hide_popup_timer_.Stop();
    toast_timer_.Stop();
    loading_start_time_.reset();
    prompt_state_ = PromptState::kShowing;
  }

  popup_controller_.Show(
      element_bounds_in_screen_space, issuer_site, email,
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
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void EmailVerificationController::OnPromptDecision(
    AutofillClient::EmailVerificationPermissionUiStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
      prompt_state_ = PromptState::kClosed;
      DismissPopup();
      return;
    case PromptState::kLoading: {
      base::TimeDelta delay = GetRemainingLoadingDuration();
      if (!delay.is_zero()) {
        if (!hide_popup_timer_.IsRunning()) {
          hide_popup_timer_.Start(
              FROM_HERE, delay,
              base::BindOnce(&EmailVerificationController::HidePopup,
                             weak_ptr_factory_.GetWeakPtr()));
        }
        return;
      }
      hide_popup_timer_.Stop();
      prompt_state_ = PromptState::kClosed;
      loading_start_time_.reset();
      DismissPopup();
      return;
    }
  }
}

void EmailVerificationController::ShowLoadingToast() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `ShowLoadingToast()` is called at the beginning of the subsequent-run
  // verification flow to initiate the loading state. Dismiss any active or
  // pending first-run popup first, then record `loading_start_time_` so the
  // minimum loading display duration is enforced when transitioning out of
  // loading in `ShowVerifiedToast()` or `ShowErrorToast()`.
  hide_popup_timer_.Stop();
  toast_timer_.Stop();
  loading_start_time_.reset();
  prompt_state_ = PromptState::kClosed;
  DismissPopup();
  StartLoadingTimer();
  if (ToastController* toast_controller = GetToastController()) {
    toast_controller->MaybeShowToast(
        ToastParams(ToastId::kEmailVerificationLoading));
  }
}

void EmailVerificationController::ShowErrorToast() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (DeferToast(
          base::BindOnce(&EmailVerificationController::ShowErrorToastImpl,
                         weak_ptr_factory_.GetWeakPtr()))) {
    return;
  }
  ShowErrorToastImpl();
}

void EmailVerificationController::ShowVerifiedToast(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (DeferToast(
          base::BindOnce(&EmailVerificationController::ShowVerifiedToastImpl,
                         weak_ptr_factory_.GetWeakPtr(), url))) {
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

BrowserWindowInterface* EmailVerificationController::GetBrowserWindowInterface()
    const {
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(&*web_contents_);
  return tab_interface ? tab_interface->GetBrowserWindowInterface() : nullptr;
}

ToastController* EmailVerificationController::GetToastController() const {
  if (!IsWebContentsVisible()) {
    return nullptr;
  }
  BrowserWindowInterface* window_interface = GetBrowserWindowInterface();
  return window_interface ? ToastController::From(window_interface) : nullptr;
}

void EmailVerificationController::ShowErrorToastImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ToastController* toast_controller = GetToastController()) {
    toast_controller->MaybeShowToast(
        ToastParams(ToastId::kEmailVerificationError));
  }
}

void EmailVerificationController::ShowVerifiedToastImpl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ToastController* toast_controller = GetToastController();
  if (!toast_controller) {
    return;
  }
  ToastParams params(ToastId::kEmailVerified);
  params.body_string_replacement_params.push_back(
      base::UTF8ToUTF16(url.host()));
  params.menu_model = std::make_unique<EmailVerifiedToastMenuModel>(
      GetBrowserWindowInterface());
  toast_controller->MaybeShowToast(std::move(params));
}

void EmailVerificationController::DismissPopup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool was_loading = popup_controller_.is_loading();
  popup_controller_.Dismiss();
  if (was_loading && IsWebContentsVisible()) {
    // The popup is activatable and took focus when the user clicked "Verify".
    // Once it closes, give focus back to the page so it returns to the form
    // field, rather than letting the platform hand activation to the
    // completion toast (whose first focusable view is its menu button).
    web_contents_->Focus();
  }
}

void EmailVerificationController::StartLoadingTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loading_start_time_ = base::TimeTicks::Now();
}

bool EmailVerificationController::DeferToast(
    base::OnceClosure show_toast_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeDelta delay = GetRemainingLoadingDuration();
  if (!delay.is_zero()) {
    toast_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&EmailVerificationController::OnToastTimerFired,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(show_toast_callback)));
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
