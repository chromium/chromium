// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verified_toast_menu_model.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

EmailVerificationController::EmailVerificationController(
    content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)) {}

EmailVerificationController::~EmailVerificationController() = default;

void EmailVerificationController::ShowPopup(
    const gfx::RectF& element_bounds_in_screen_space,
    const net::SchemefulSite& issuer_site,
    const std::u16string& email,
    base::OnceCallback<
        void(AutofillClient::EmailVerificationPermissionUiStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hide_popup_timer_.Stop();
  toast_timer_.Stop();
  loading_start_time_.reset();

  if (!popup_controller_) {
    popup_controller_ =
        std::make_unique<EmailVerificationPopupController>(&*web_contents_);
  }
  popup_controller_->Show(
      element_bounds_in_screen_space, issuer_site, email,
      base::BindOnce(&EmailVerificationController::OnPopupPermissionDecision,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void EmailVerificationController::OnPopupPermissionDecision(
    base::OnceCallback<
        void(AutofillClient::EmailVerificationPermissionUiStatus)> callback,
    AutofillClient::EmailVerificationPermissionUiStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the user confirmed the first-run prompt (`kAllowed`), the popup view has
  // transitioned to its loading spinner, and the delegate will initiate
  // background token retrieval upon receiving `callback`.
  // We capture `loading_start_time_` here to track the start of the first-run
  // loading state. This ensures that `HidePopup()` and subsequent completion
  // toasts (verified / error) enforce `kMinimumLoadingDuration` (800ms) before
  // tearing down the loading UI, preventing visual flickering on fast network
  // responses.
  if (status == AutofillClient::EmailVerificationPermissionUiStatus::kAllowed) {
    loading_start_time_ = base::TimeTicks::Now();
  }
  std::move(callback).Run(status);
}

void EmailVerificationController::HidePopup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!popup_controller_) {
    return;
  }
  if (popup_controller_->is_loading()) {
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
  }
  hide_popup_timer_.Stop();
  const bool was_loading = popup_controller_->is_loading();
  popup_controller_->Dismiss();
  if (was_loading) {
    // The popup is activatable and took focus when the user clicked "Verify".
    // Once it closes, give focus back to the page so it returns to the form
    // field, rather than letting the platform hand activation to the
    // completion toast (whose first focusable view is its menu button).
    web_contents_->Focus();
    if (!toast_timer_.IsRunning()) {
      loading_start_time_.reset();
    }
  }
}

void EmailVerificationController::ShowLoadingToast() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There is no need to check any remaining loading duration timer here
  // because `ShowLoadingToast()` is called at the beginning of the
  // subsequent-run verification flow to initiate the loading state. We
  // record `loading_start_time_` here; the minimum loading display duration
  // is only enforced later when transitioning out of loading in
  // `ShowVerifiedToast()` or `ShowErrorToast()`.
  loading_start_time_ = base::TimeTicks::Now();
  toast_timer_.Stop();
  if (ToastController* toast_controller = GetToastController()) {
    toast_controller->MaybeShowToast(
        ToastParams(ToastId::kEmailVerificationLoading));
  }
}

bool EmailVerificationController::DeferToast(
    base::OnceClosure show_toast_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeDelta delay = GetRemainingLoadingDuration();
  if (!delay.is_zero()) {
    toast_timer_.Start(FROM_HERE, delay, std::move(show_toast_callback));
    return true;
  }
  toast_timer_.Stop();
  loading_start_time_.reset();
  return false;
}

void EmailVerificationController::ShowErrorToast() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (DeferToast(base::BindOnce(&EmailVerificationController::ShowErrorToast,
                                weak_ptr_factory_.GetWeakPtr()))) {
    return;
  }

  if (ToastController* toast_controller = GetToastController()) {
    toast_controller->MaybeShowToast(
        ToastParams(ToastId::kEmailVerificationError));
  }
}

void EmailVerificationController::ShowVerifiedToast(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (DeferToast(base::BindOnce(&EmailVerificationController::ShowVerifiedToast,
                                weak_ptr_factory_.GetWeakPtr(), url))) {
    return;
  }

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

BrowserWindowInterface*
EmailVerificationController::GetBrowserWindowInterface() {
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(&*web_contents_);
  return tab_interface ? tab_interface->GetBrowserWindowInterface() : nullptr;
}

ToastController* EmailVerificationController::GetToastController() {
  BrowserWindowInterface* window_interface = GetBrowserWindowInterface();
  return window_interface ? ToastController::From(window_interface) : nullptr;
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
