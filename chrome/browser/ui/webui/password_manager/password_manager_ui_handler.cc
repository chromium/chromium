// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui_handler.h"

#include <optional>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
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

password_manager::mojom::ImportResultsStatus ToMojomImportResultsStatus(
    extensions::api::passwords_private::ImportResultsStatus status) {
  switch (status) {
    case extensions::api::passwords_private::ImportResultsStatus::kNone:
    case extensions::api::passwords_private::ImportResultsStatus::kUnknownError:
      return password_manager::mojom::ImportResultsStatus::kUnknownError;
    case extensions::api::passwords_private::ImportResultsStatus::kSuccess:
      return password_manager::mojom::ImportResultsStatus::kSuccess;
    case extensions::api::passwords_private::ImportResultsStatus::kIoError:
      return password_manager::mojom::ImportResultsStatus::kIoError;
    case extensions::api::passwords_private::ImportResultsStatus::kBadFormat:
      return password_manager::mojom::ImportResultsStatus::kBadFormat;
    case extensions::api::passwords_private::ImportResultsStatus::kDismissed:
      return password_manager::mojom::ImportResultsStatus::kDismissed;
    case extensions::api::passwords_private::ImportResultsStatus::kMaxFileSize:
      return password_manager::mojom::ImportResultsStatus::kMaxFileSize;
    case extensions::api::passwords_private::ImportResultsStatus::
        kImportAlreadyActive:
      return password_manager::mojom::ImportResultsStatus::kImportAlreadyActive;
    case extensions::api::passwords_private::ImportResultsStatus::
        kNumPasswordsExceeded:
      return password_manager::mojom::ImportResultsStatus::
          kNumPasswordsExceeded;
    case extensions::api::passwords_private::ImportResultsStatus::kConflicts:
      return password_manager::mojom::ImportResultsStatus::kConflicts;
  }
  NOTREACHED();
}

password_manager::mojom::ImportEntryStatus ToMojomImportEntryStatus(
    extensions::api::passwords_private::ImportEntryStatus status) {
  switch (status) {
    case extensions::api::passwords_private::ImportEntryStatus::kNone:
    case extensions::api::passwords_private::ImportEntryStatus::kUnknownError:
      return password_manager::mojom::ImportEntryStatus::kUnknownError;
    case extensions::api::passwords_private::ImportEntryStatus::
        kMissingPassword:
      return password_manager::mojom::ImportEntryStatus::kMissingPassword;
    case extensions::api::passwords_private::ImportEntryStatus::kMissingUrl:
      return password_manager::mojom::ImportEntryStatus::kMissingUrl;
    case extensions::api::passwords_private::ImportEntryStatus::kInvalidUrl:
      return password_manager::mojom::ImportEntryStatus::kInvalidUrl;
    case extensions::api::passwords_private::ImportEntryStatus::kNonAsciiUrl:
      return password_manager::mojom::ImportEntryStatus::kNonAsciiUrl;
    case extensions::api::passwords_private::ImportEntryStatus::kLongUrl:
      return password_manager::mojom::ImportEntryStatus::kLongUrl;
    case extensions::api::passwords_private::ImportEntryStatus::kLongPassword:
      return password_manager::mojom::ImportEntryStatus::kLongPassword;
    case extensions::api::passwords_private::ImportEntryStatus::kLongUsername:
      return password_manager::mojom::ImportEntryStatus::kLongUsername;
    case extensions::api::passwords_private::ImportEntryStatus::
        kConflictProfile:
      return password_manager::mojom::ImportEntryStatus::kConflictProfile;
    case extensions::api::passwords_private::ImportEntryStatus::
        kConflictAccount:
      return password_manager::mojom::ImportEntryStatus::kConflictAccount;
    case extensions::api::passwords_private::ImportEntryStatus::kLongNote:
      return password_manager::mojom::ImportEntryStatus::kLongNote;
    case extensions::api::passwords_private::ImportEntryStatus::
        kLongConcatenatedNote:
      return password_manager::mojom::ImportEntryStatus::kLongConcatenatedNote;
    case extensions::api::passwords_private::ImportEntryStatus::kValid:
      return password_manager::mojom::ImportEntryStatus::kValid;
  }
  NOTREACHED();
}

password_manager::mojom::ImportEntryPtr ToMojomImportEntry(
    const extensions::api::passwords_private::ImportEntry& entry) {
  return password_manager::mojom::ImportEntry::New(
      /*status=*/ToMojomImportEntryStatus(entry.status),
      /*url=*/entry.url,
      /*username=*/entry.username,
      /*password=*/entry.password,
      /*id=*/entry.id);
}

password_manager::mojom::ImportResultsPtr ToMojomImportResults(
    const extensions::api::passwords_private::ImportResults& results) {
  return password_manager::mojom::ImportResults::New(
      /*status=*/ToMojomImportResultsStatus(results.status),
      /*number_imported=*/results.number_imported,
      /*displayed_entries=*/
      base::ToVector(results.displayed_entries, &ToMojomImportEntry),
      /*file_name=*/results.file_name);
}

extensions::api::passwords_private::PasswordStoreSet ConvertPasswordStoreSet(
    password_manager::mojom::PasswordStoreSet to_store) {
  switch (to_store) {
    case password_manager::mojom::PasswordStoreSet::kDevice:
      return extensions::api::passwords_private::PasswordStoreSet::kDevice;
    case password_manager::mojom::PasswordStoreSet::kAccount:
      return extensions::api::passwords_private::PasswordStoreSet::kAccount;
    case password_manager::mojom::PasswordStoreSet::kDeviceAndAccount:
      return extensions::api::passwords_private::PasswordStoreSet::
          kDeviceAndAccount;
  }
  NOTREACHED();
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

void PasswordManagerUIHandler::StopPasswordChange() {
  CHECK(base::FeatureList::IsEnabled(
      password_manager::features::kPasswordCheckupPrototype));
  CHECK(web_contents_);
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto* service = PasswordChangeServiceFactory::GetForProfile(profile);
  if (service) {
    service->StopPasswordChangeFromCheckup();
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

void PasswordManagerUIHandler::ImportPasswords(
    password_manager::mojom::PasswordStoreSet to_store,
    ImportPasswordsCallback callback) {
  passwords_private_delegate_->ImportPasswords(
      ConvertPasswordStoreSet(to_store),
      base::BindOnce(&ToMojomImportResults).Then(std::move(callback)),
      web_contents_);
}

void PasswordManagerUIHandler::ContinueImport(
    const std::vector<int32_t>& selected_ids,
    ContinueImportCallback callback) {
  passwords_private_delegate_->ContinueImport(
      selected_ids,
      base::BindOnce(&ToMojomImportResults).Then(std::move(callback)));
}

void PasswordManagerUIHandler::StartTrustedVaultUnlock() {
  if (BrowserWindowInterface* browser =
          GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
              web_contents_)) {
    OpenTabForSyncKeyRetrieval(
        browser, trusted_vault::TrustedVaultUserActionTriggerForUMA::kSettings);
  }
}
