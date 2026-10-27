// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_mojom_traits.h"

#include "base/notreached.h"
#include "chrome/browser/ui/webui/password_manager/password_manager.mojom-shared.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
password_manager::mojom::ExportProgressStatus
EnumTraits<password_manager::mojom::ExportProgressStatus,
           password_manager::ExportProgressStatus>::
    ToMojom(password_manager::ExportProgressStatus input) {
  switch (input) {
    case password_manager::ExportProgressStatus::kNotStarted:
      return password_manager::mojom::ExportProgressStatus::kNotStarted;
    case password_manager::ExportProgressStatus::kInProgress:
      return password_manager::mojom::ExportProgressStatus::kInProgress;
    case password_manager::ExportProgressStatus::kSucceeded:
      return password_manager::mojom::ExportProgressStatus::kSucceeded;
    case password_manager::ExportProgressStatus::kFailedCancelled:
      return password_manager::mojom::ExportProgressStatus::kFailed;
    case password_manager::ExportProgressStatus::kFailedWrite:
      return password_manager::mojom::ExportProgressStatus::kFailedWrite;
  }
  NOTREACHED();
}

// static
password_manager::ExportProgressStatus
EnumTraits<password_manager::mojom::ExportProgressStatus,
           password_manager::ExportProgressStatus>::
    FromMojom(password_manager::mojom::ExportProgressStatus input) {
  switch (input) {
    case password_manager::mojom::ExportProgressStatus::kNotStarted:
      return password_manager::ExportProgressStatus::kNotStarted;
    case password_manager::mojom::ExportProgressStatus::kInProgress:
      return password_manager::ExportProgressStatus::kInProgress;
    case password_manager::mojom::ExportProgressStatus::kSucceeded:
      return password_manager::ExportProgressStatus::kSucceeded;
    case password_manager::mojom::ExportProgressStatus::kFailed:
      return password_manager::ExportProgressStatus::kFailedCancelled;
    case password_manager::mojom::ExportProgressStatus::kFailedWrite:
      return password_manager::ExportProgressStatus::kFailedWrite;
  }
  NOTREACHED();
}

// static
password_manager::mojom::PasswordAutomaticChangeState
EnumTraits<password_manager::mojom::PasswordAutomaticChangeState,
           PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState>::
    ToMojom(
        PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState input) {
  switch (input) {
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kInactive:
      return password_manager::mojom::PasswordAutomaticChangeState::kInactive;
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kAttemptingSignIn:
      return password_manager::mojom::PasswordAutomaticChangeState::
          kAttemptingSignIn;
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kChangingPassword:
      return password_manager::mojom::PasswordAutomaticChangeState::
          kChangingPassword;
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kConfirmingChangedPassword:
      return password_manager::mojom::PasswordAutomaticChangeState::
          kConfirmingChangedPassword;
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kPasswordChangedSuccessfully:
      return password_manager::mojom::PasswordAutomaticChangeState::
          kPasswordChangedSuccessfully;
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kError:
      return password_manager::mojom::PasswordAutomaticChangeState::kError;
  }
  NOTREACHED();
}

// static
PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState
EnumTraits<password_manager::mojom::PasswordAutomaticChangeState,
           PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState>::
    FromMojom(password_manager::mojom::PasswordAutomaticChangeState input) {
  switch (input) {
    case password_manager::mojom::PasswordAutomaticChangeState::kInactive:
      return PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
          kInactive;
    case password_manager::mojom::PasswordAutomaticChangeState::
        kAttemptingSignIn:
      return PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
          kAttemptingSignIn;
    case password_manager::mojom::PasswordAutomaticChangeState::
        kChangingPassword:
      return PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
          kChangingPassword;
    case password_manager::mojom::PasswordAutomaticChangeState::
        kConfirmingChangedPassword:
      return PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
          kConfirmingChangedPassword;
    case password_manager::mojom::PasswordAutomaticChangeState::
        kPasswordChangedSuccessfully:
      return PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
          kPasswordChangedSuccessfully;
    case password_manager::mojom::PasswordAutomaticChangeState::kError:
      return PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
          kError;
  }
  NOTREACHED();
}

// static
password_manager::mojom::PasswordManagerActionableError
EnumTraits<password_manager::mojom::PasswordManagerActionableError,
           password_manager::ActionableError>::
    ToMojom(password_manager::ActionableError input) {
  switch (input) {
    case password_manager::ActionableError::kNoError:
      return password_manager::mojom::PasswordManagerActionableError::kNoError;
    case password_manager::ActionableError::kInactionable:
      return password_manager::mojom::PasswordManagerActionableError::
          kInactionable;
    case password_manager::ActionableError::kInactionableTemporaryError:
      return password_manager::mojom::PasswordManagerActionableError::
          kInactionableTemporaryError;
    case password_manager::ActionableError::kSignInNeeded:
      return password_manager::mojom::PasswordManagerActionableError::
          kSignInNeeded;
    case password_manager::ActionableError::kKeychainError:
      return password_manager::mojom::PasswordManagerActionableError::
          kKeychainError;
    case password_manager::ActionableError::kTrustedVaultKeyNeeded:
      return password_manager::mojom::PasswordManagerActionableError::
          kTrustedVaultKeyNeeded;
    case password_manager::ActionableError::kNeedsPassphrase:
      return password_manager::mojom::PasswordManagerActionableError::
          kNeedsPassphrase;
  }
  NOTREACHED();
}

// static
password_manager::ActionableError
EnumTraits<password_manager::mojom::PasswordManagerActionableError,
           password_manager::ActionableError>::
    FromMojom(password_manager::mojom::PasswordManagerActionableError input) {
  switch (input) {
    case password_manager::mojom::PasswordManagerActionableError::kNoError:
      return password_manager::ActionableError::kNoError;
    case password_manager::mojom::PasswordManagerActionableError::kInactionable:
      return password_manager::ActionableError::kInactionable;
    case password_manager::mojom::PasswordManagerActionableError::
        kInactionableTemporaryError:
      return password_manager::ActionableError::kInactionableTemporaryError;
    case password_manager::mojom::PasswordManagerActionableError::kSignInNeeded:
      return password_manager::ActionableError::kSignInNeeded;
    case password_manager::mojom::PasswordManagerActionableError::
        kKeychainError:
      return password_manager::ActionableError::kKeychainError;
    case password_manager::mojom::PasswordManagerActionableError::
        kTrustedVaultKeyNeeded:
      return password_manager::ActionableError::kTrustedVaultKeyNeeded;
    case password_manager::mojom::PasswordManagerActionableError::
        kNeedsPassphrase:
      return password_manager::ActionableError::kNeedsPassphrase;
  }
  NOTREACHED();
}

// static
bool StructTraits<password_manager::mojom::DomainInfoDataView,
                  password_manager::CredentialUIEntry::DomainInfo>::
    Read(password_manager::mojom::DomainInfoDataView data,
         password_manager::CredentialUIEntry::DomainInfo* out) {
  if (!data.ReadHumanReadableName(&out->name)) {
    return false;
  }
  if (!data.ReadUrl(&out->url)) {
    return false;
  }
  if (!data.ReadSignonRealm(&out->signon_realm)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<password_manager::mojom::ActorLoginPermissionDataView,
                  password_manager::ActorLoginPermission>::
    Read(password_manager::mojom::ActorLoginPermissionDataView data,
         password_manager::ActorLoginPermission* out) {
  if (!data.ReadDomainInfo(&out->domain_info)) {
    return false;
  }
  if (!data.ReadFaviconUrl(&out->favicon_url)) {
    return false;
  }
  if (!data.ReadUsername(&out->username)) {
    return false;
  }
  return true;
}

}  // namespace mojo
