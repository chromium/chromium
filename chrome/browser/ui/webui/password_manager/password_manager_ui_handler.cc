// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui_handler.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/password_manager/password_manager.mojom.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/password_manager/core/browser/export/export_progress_status.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/actor_login_permission.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

password_manager::mojom::ExportProgressStatus ToExportProgressMojomStatus(
    password_manager::ExportProgressStatus status) {
  switch (status) {
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
}

password_manager::mojom::ExportProgressStatus ToExportProgressMojomStatus(
    extensions::api::passwords_private::ExportProgressStatus status) {
  switch (status) {
    case extensions::api::passwords_private::ExportProgressStatus::kNotStarted:
    case extensions::api::passwords_private::ExportProgressStatus::kNone:
      return password_manager::mojom::ExportProgressStatus::kNotStarted;
    case extensions::api::passwords_private::ExportProgressStatus::kInProgress:
      return password_manager::mojom::ExportProgressStatus::kInProgress;
    case extensions::api::passwords_private::ExportProgressStatus::kSucceeded:
      return password_manager::mojom::ExportProgressStatus::kSucceeded;
    case extensions::api::passwords_private::ExportProgressStatus::
        kFailedCancelled:
      return password_manager::mojom::ExportProgressStatus::kFailed;
    case extensions::api::passwords_private::ExportProgressStatus::
        kFailedWriteFailed:
      return password_manager::mojom::ExportProgressStatus::kFailedWrite;
  }
}

password_manager::mojom::ExportPasswordsResult ToExportPasswordsMojomResult(
    extensions::PasswordsPrivateDelegate::ExportPasswordsResult result) {
  switch (result) {
    case extensions::PasswordsPrivateDelegate::ExportPasswordsResult::kSuccess:
      return password_manager::mojom::ExportPasswordsResult::kSuccess;
    case extensions::PasswordsPrivateDelegate::ExportPasswordsResult::
        kInProgress:
      return password_manager::mojom::ExportPasswordsResult::kInProgress;
    case extensions::PasswordsPrivateDelegate::ExportPasswordsResult::
        kReauthFailed:
      return password_manager::mojom::ExportPasswordsResult::kReauthFailed;
  }
}

password_manager::mojom::PasswordManagerActionableError ToActionableMojomError(
    password_manager::ActionableError error) {
  using password_manager::mojom::PasswordManagerActionableError;
  switch (error) {
    case password_manager::ActionableError::kNoError:
      return PasswordManagerActionableError::kNoError;
    case password_manager::ActionableError::kInactionable:
      return PasswordManagerActionableError::kInactionable;
    case password_manager::ActionableError::kInactionableTemporaryError:
      return PasswordManagerActionableError::kInactionableTemporaryError;
    case password_manager::ActionableError::kSignInNeeded:
      return PasswordManagerActionableError::kSignInNeeded;
    case password_manager::ActionableError::kKeychainError:
      return PasswordManagerActionableError::kKeychainError;
    case password_manager::ActionableError::kTrustedVaultKeyNeeded:
      return PasswordManagerActionableError::kTrustedVaultKeyNeeded;
    case password_manager::ActionableError::kNeedsPassphrase:
      return PasswordManagerActionableError::kNeedsPassphrase;
  }
}

password_manager::mojom::PasswordAutomaticChangeState
ToPasswordAutomaticChangeMojomState(
    PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState state) {
  using password_manager::mojom::PasswordAutomaticChangeState;
  switch (state) {
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kInactive:
      return PasswordAutomaticChangeState::kInactive;
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kAttemptingSignIn:
      return PasswordAutomaticChangeState::kAttemptingSignIn;
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kChangingPassword:
      return PasswordAutomaticChangeState::kChangingPassword;
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kConfirmingChangedPassword:
      return PasswordAutomaticChangeState::kConfirmingChangedPassword;
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kPasswordChangedSuccessfully:
      return PasswordAutomaticChangeState::kPasswordChangedSuccessfully;
    case PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
        kError:
      return PasswordAutomaticChangeState::kError;
  }
}

}  // namespace

PasswordManagerUIHandler::PasswordManagerUIHandler(
    mojo::PendingReceiver<password_manager::mojom::PageHandler> receiver,
    mojo::PendingRemote<password_manager::mojom::Page> page,
    scoped_refptr<extensions::PasswordsPrivateDelegate>
        passwords_private_delegate,
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      passwords_private_delegate_(std::move(passwords_private_delegate)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  passwords_private_delegate_observation_.Observe(
      passwords_private_delegate_.get());
}

PasswordManagerUIHandler::~PasswordManagerUIHandler() = default;

void PasswordManagerUIHandler::ExtendAuthValidity() {
  passwords_private_delegate_->RestartAuthTimer();
}

void PasswordManagerUIHandler::DeleteAllPasswordManagerData(
    DeleteAllPasswordManagerDataCallback callback) {
  passwords_private_delegate_->DeleteAllPasswordManagerData(
      std::move(callback));
}

void PasswordManagerUIHandler::CopyPlaintextBackupPassword(
    int id,
    CopyPlaintextBackupPasswordCallback callback) {
  passwords_private_delegate_->CopyPlaintextBackupPassword(id,
                                                           std::move(callback));
}

void PasswordManagerUIHandler::RemoveBackupPassword(int id) {
  passwords_private_delegate_->RemoveBackupPassword(id);
}

void PasswordManagerUIHandler::RemovePasswordException(int id) {
  passwords_private_delegate_->RemovePasswordException(id);
}

void PasswordManagerUIHandler::StartBulkPasswordCheck() {
  passwords_private_delegate_->StartPasswordCheck(base::DoNothing());
}

void PasswordManagerUIHandler::MovePasswordsToAccount(
    const std::vector<int>& ids) {
  passwords_private_delegate_->MovePasswordsToAccount(ids);
}

void PasswordManagerUIHandler::ResetImporter(bool delete_file,
                                             ResetImporterCallback callback) {
  passwords_private_delegate_->ResetImporter(delete_file);
  std::move(callback).Run();
}

void PasswordManagerUIHandler::GetActorLoginPermissions(
    GetActorLoginPermissionsCallback callback) {
  std::vector<password_manager::mojom::ActorLoginPermissionPtr> result;
  syncer::SyncService* sync_service = SyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  for (const auto& site :
       GetSavedPasswordsPresenter()->GetActorLoginPermissions(sync_service)) {
    auto url = password_manager::mojom::DomainInfo::New(
        site.domain_info.name, site.domain_info.url,
        site.domain_info.signon_realm);
    result.push_back(password_manager::mojom::ActorLoginPermission::New(
        std::move(url), site.favicon_url, base::UTF16ToUTF8(site.username)));
  }
  std::move(callback).Run(std::move(result));
}

void PasswordManagerUIHandler::RevokeActorLoginPermission(
    password_manager::mojom::ActorLoginPermissionPtr site) {
  GetSavedPasswordsPresenter()->RevokeActorLoginPermission(
      site->domain_info->signon_realm, site->username);
}

void PasswordManagerUIHandler::ChangePasswordManagerPin(
    ChangePasswordManagerPinCallback callback) {
  passwords_private_delegate_->ChangePasswordManagerPin(web_contents_,
                                                        std::move(callback));
}

void PasswordManagerUIHandler::IsPasswordManagerPinAvailable(
    IsPasswordManagerPinAvailableCallback callback) {
  passwords_private_delegate_->IsPasswordManagerPinAvailable(
      web_contents_, std::move(callback));
}

void PasswordManagerUIHandler::ShowAddShortcutDialog() {
  passwords_private_delegate_->ShowAddShortcutDialog(web_contents_);
}

password_manager::SavedPasswordsPresenter*
PasswordManagerUIHandler::GetSavedPasswordsPresenter() {
  return passwords_private_delegate_->GetSavedPasswordsPresenter();
}

void PasswordManagerUIHandler::IsAccountStorageActive(
    IsAccountStorageActiveCallback callback) {
  bool result = passwords_private_delegate_->IsAccountStorageActive();
  std::move(callback).Run(result);
}

void PasswordManagerUIHandler::SetAccountStorageEnabled(bool enabled) {
  passwords_private_delegate_->SetAccountStorageEnabled(enabled);
}

void PasswordManagerUIHandler::ShouldShowAccountStorageSettingToggle(
    ShouldShowAccountStorageSettingToggleCallback callback) {
  std::move(callback).Run(
      passwords_private_delegate_->ShouldShowAccountStorageSettingToggle());
}

void PasswordManagerUIHandler::SwitchBiometricAuthBeforeFillingState(
    SwitchBiometricAuthBeforeFillingStateCallback callback) {
  passwords_private_delegate_->SwitchBiometricAuthBeforeFillingState(
      std::move(callback));
}

void PasswordManagerUIHandler::StartPasswordChange(int credential_id) {
  CHECK(base::FeatureList::IsEnabled(
      password_manager::features::kPasswordCheckupPrototype));
  CHECK(web_contents_);
  auto credential =
      passwords_private_delegate_->GetCredentialFromId(credential_id);
  if (!credential) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto* service = PasswordChangeServiceFactory::GetForProfile(profile);
  if (service) {
    service->StartPasswordChangeFromCheckup(
        *credential, web_contents_,
        base::BindRepeating(&PasswordManagerUIHandler::OnPasswordAutomaticChangeStateUpdated,
                            weak_ptr_factory_.GetWeakPtr(), credential_id));
  }
}

void PasswordManagerUIHandler::OnPasswordAutomaticChangeStateUpdated(
    int credential_id,
    PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState state) {
  page_->OnPasswordAutomaticChangeStateUpdated(
      credential_id, ToPasswordAutomaticChangeMojomState(state));
}

void PasswordManagerUIHandler::GetPasswordManagerActionableError(
    GetPasswordManagerActionableErrorCallback callback) {
  std::move(callback).Run(ToActionableMojomError(
      passwords_private_delegate_->GetActionableError()));
}

void PasswordManagerUIHandler::ShowLastExportedFileInShell() {
  passwords_private_delegate_->ShowLastExportedFileInShell(web_contents_);
}

void PasswordManagerUIHandler::DisconnectCloudAuthenticator(
    DisconnectCloudAuthenticatorCallback callback) {
  passwords_private_delegate_->DisconnectCloudAuthenticator(
      std::move(callback));
}

void PasswordManagerUIHandler::IsConnectedToCloudAuthenticator(
    IsConnectedToCloudAuthenticatorCallback callback) {
  std::move(callback).Run(
      passwords_private_delegate_->IsConnectedToCloudAuthenticator());
}

void PasswordManagerUIHandler::UndoRemoveSavedPasswordOrException() {
  passwords_private_delegate_->UndoRemoveSavedPasswordOrException();
}

void PasswordManagerUIHandler::RequestPasswordsExport(
    RequestPasswordsExportCallback callback) {
  passwords_private_delegate_->ExportPasswords(
      base::BindOnce(
          [](RequestPasswordsExportCallback callback,
             extensions::PasswordsPrivateDelegate::ExportPasswordsResult
                 result) {
            std::move(callback).Run(ToExportPasswordsMojomResult(result));
          },
          std::move(callback)),
      web_contents_);
}

void PasswordManagerUIHandler::GetPasswordsExportProgress(
    GetPasswordsExportProgressCallback callback) {
  std::move(callback).Run(ToExportProgressMojomStatus(
      passwords_private_delegate_->GetExportProgressStatus()));
}

void PasswordManagerUIHandler::OnPasswordsExportProgress(
    password_manager::ExportProgressStatus status,
    const std::string& folder_name) {
  page_->OnPasswordsExportProgress(
      ToExportProgressMojomStatus(status),
      folder_name.empty() ? std::nullopt : std::make_optional(folder_name));
}
