// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_change_controller.h"

#include "build/build_config.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LeakDialogType;
}  // namespace

CredentialLeakDialogChangeController::CredentialLeakDialogChangeController(
    PasswordsLeakDialogDelegate* delegate,
    password_manager::metrics_util::LeakDialogType dialog_type)
    : CredentialLeakDialogBaseController(delegate, dialog_type) {}

void CredentialLeakDialogChangeController::OnAcceptDialog() {
  LogLeakDialogTypeAndDismissalReason(getDialogType(),
                                      LeakDialogDismissalReason::kClickedOk);
  getDelegate()->OnLeakDialogHidden();
}

std::u16string CredentialLeakDialogChangeController::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_OK);
}

std::u16string CredentialLeakDialogChangeController::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

std::u16string CredentialLeakDialogChangeController::GetDescription() const {
#if BUILDFLAG(IS_IOS)
  const bool uses_password_manager_updated_naming =
      base::FeatureList::IsEnabled(
          password_manager::features::kIOSEnablePasswordManagerBrandingUpdate);
  const bool uses_password_manager_google_branding = true;
#elif BUILDFLAG(IS_ANDROID)
  const bool uses_password_manager_updated_naming =
      password_manager::features::UsesUnifiedPasswordManagerUi();
  const bool uses_password_manager_google_branding =
      password_manager_util::UsesPasswordManagerGoogleBranding(
          IsSyncingPasswordsNormally(leak_type));
#else
  // TODO(crbug.com/1309480): Update to support Desktop branding.
  const bool uses_password_manager_updated_naming = false;
  const bool uses_password_manager_google_branding = false;
#endif
  if (uses_password_manager_updated_naming) {
    return l10n_util::GetStringUTF16(
        uses_password_manager_google_branding
            ? IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_BRANDED
            : IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_NON_BRANDED);
  } else {
    return l10n_util::GetStringUTF16(
        IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE);
  }
}

std::u16string CredentialLeakDialogChangeController::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_CREDENTIAL_LEAK_TITLE_CHANGE);
}

bool CredentialLeakDialogChangeController::ShouldCheckPasswords() const {
  return false;
}

bool CredentialLeakDialogChangeController::ShouldOfferAutomatedPasswordChange()
    const {
  return false;
}

bool CredentialLeakDialogChangeController::ShouldShowCancelButton() const {
  return false;
}
