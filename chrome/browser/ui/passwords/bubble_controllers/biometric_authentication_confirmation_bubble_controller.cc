// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/biometric_authentication_confirmation_bubble_controller.h"

#include "base/notreached.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

BiometricAuthenticationConfirmationBubbleController::
    BiometricAuthenticationConfirmationBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          password_manager::metrics_util::
              AUTOMATIC_BIOMETRIC_AUTHENTICATION_CONFIRMATION) {}

BiometricAuthenticationConfirmationBubbleController::
    ~BiometricAuthenticationConfirmationBubbleController() {
  // Make sure the interactions are reported even if Views didn't notify the
  // controller about the bubble being closed.
  OnBubbleClosing();
}

int BiometricAuthenticationConfirmationBubbleController::GetImageID(
    bool dark) const {
  return dark ? IDR_BIOMETRIC_AUTHENTICATION_CONFIRMATION_PROMPT_DARK
              : IDR_BIOMETRIC_AUTHENTICATION_CONFIRMATION_PROMPT_LIGHT;
}

std::u16string BiometricAuthenticationConfirmationBubbleController::GetTitle()
    const {
#if BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_CONFIRMATION_TITLE_MAC);
#elif BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_CONFIRMATION_TITLE_WIN);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_CONFIRMATION_TITLE_CHROMEOS);
#else
  NOTIMPLEMENTED();
#endif
}

void BiometricAuthenticationConfirmationBubbleController::
    OnNavigateToSettingsLinkClicked() {
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(
        password_manager::ManagePasswordsReferrer::
            kBiometricAuthenticationBeforeFillingDialog);
  }
}
