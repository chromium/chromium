// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/biometric_authentication_for_filling_bubble_controller.h"

#include <string>

#include "base/notreached.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void OnReauthCompleted(PrefService* prefs,
                       base::WeakPtr<PasswordsModelDelegate> delegate,
                       bool success) {
  base::UmaHistogramBoolean(
      "PasswordManager.BiometricAuthenticationPromo.AuthenticationResult",
      success);
  if (!success) {
    return;
  }
  DCHECK(prefs);
  prefs->SetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo, true);
  prefs->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);

  if (delegate) {
    delegate->ShowBiometricActivationConfirmation();
  }
}

password_manager::metrics_util::UIDisplayDisposition GetDisplayDisposition(
    PasswordBubbleControllerBase::DisplayReason display_reason) {
  switch (display_reason) {
    case PasswordBubbleControllerBase::DisplayReason::kUserAction:
      return password_manager::metrics_util::
          MANUAL_BIOMETRIC_AUTHENTICATION_FOR_FILLING;
    case PasswordBubbleControllerBase::DisplayReason::kAutomatic:
      return password_manager::metrics_util::
          AUTOMATIC_BIOMETRIC_AUTHENTICATION_FOR_FILLING;
  }
}

}  // namespace

BiometricAuthenticationForFillingBubbleController::
    BiometricAuthenticationForFillingBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate,
        PrefService* prefs,
        PasswordBubbleControllerBase::DisplayReason display_reason)
    : PasswordBubbleControllerBase(std::move(delegate),
                                   GetDisplayDisposition(display_reason)),
      prefs_(prefs) {}

BiometricAuthenticationForFillingBubbleController::
    ~BiometricAuthenticationForFillingBubbleController() {
  // Make sure the interactions are reported even if Views didn't notify the
  // controller about the bubble being closed.
  OnBubbleClosing();
}

std::u16string BiometricAuthenticationForFillingBubbleController::GetBody()
    const {
#if BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_FOR_FILLING_PROMO_MESSAGE_MAC);
#elif BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_FOR_FILLING_PROMO_MESSAGE_WIN);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_FOR_FILLING_PROMO_MESSAGE_CHROMEOS);
#else
  NOTIMPLEMENTED();
#endif
}

std::u16string
BiometricAuthenticationForFillingBubbleController::GetContinueButtonText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_FOR_FILLING_PROMO_ACCEPT_BUTTON);
}

std::u16string
BiometricAuthenticationForFillingBubbleController::GetNoThanksButtonText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_FOR_FILLING_PROMO_CANCEL_BUTTON);
}

int BiometricAuthenticationForFillingBubbleController::GetImageID(
    bool dark) const {
  return dark ? IDR_BIOMETRIC_AUTHENTICATION_PROMPT_DARK
              : IDR_BIOMETRIC_AUTHENTICATION_PROMPT_LIGHT;
}

void BiometricAuthenticationForFillingBubbleController::OnAccepted() {
  base::OnceCallback<void(bool)> on_reauth_completed =
      base::BindOnce(OnReauthCompleted, prefs_, delegate_);
  accept_clicked_ = true;
  std::u16string message;
#if BUILDFLAG(IS_MAC)
  message = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_TURN_ON_FILLING_REAUTH_MAC);
#elif BUILDFLAG(IS_WIN)
  message = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_TURN_ON_FILLING_REAUTH_WIN);
#endif
  // TODO(crbug.com/367336383): Add ChromeOS specific string.
  delegate_->AuthenticateUserWithMessage(message,
                                         std::move(on_reauth_completed));
}

void BiometricAuthenticationForFillingBubbleController::OnCanceled() {
  prefs_->SetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo, true);
  delegate_->OnBiometricAuthBeforeFillingDeclined();
}

std::u16string BiometricAuthenticationForFillingBubbleController::GetTitle()
    const {
#if BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_FOR_FILLING_PROMO_TITLE_MAC);
#elif BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_FOR_FILLING_PROMO_TITLE_WIN);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_FOR_FILLING_PROMO_TITLE_CHROMEOS);
#else
  NOTIMPLEMENTED();
#endif
}

void BiometricAuthenticationForFillingBubbleController::ReportInteractions() {
  base::UmaHistogramBoolean(
      "PasswordBubble.BiometricAuthenticationPromo.AcceptClicked",
      accept_clicked_);
}
