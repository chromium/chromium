// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_controller.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_service.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

RemoteActorCredentialSharingController::RemoteActorCredentialSharingController(
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    PasswordStoreInterface* profile_store,
    PasswordStoreInterface* account_store,
    RemoteActorCredentialSharingService* sharing_service,
    Delegate* delegate)
    : identity_manager_(identity_manager),
      sync_service_(sync_service),
      profile_store_(profile_store),
      account_store_(account_store),
      sharing_service_(sharing_service),
      delegate_(raw_ref<Delegate>::from_ptr(delegate)) {}

RemoteActorCredentialSharingController::
    ~RemoteActorCredentialSharingController() = default;

void RemoteActorCredentialSharingController::Start(std::string_view gaia_id,
                                                   std::string_view domain,
                                                   std::string_view task_id,
                                                   ResultCallback callback) {
  CHECK(callback_.is_null());
  gaia_id_ = gaia_id;
  origin_ = url::Origin::Create(GURL(base::StrCat({"https://", domain})));
  task_id_ = task_id;
  callback_ = std::move(callback);

  if (!VerifyUserIdentityAndSyncState(gaia_id)) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid);
    RespondWithError(
        RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid);
    return;
  }

  QueryPasswordStores(domain);
}

bool RemoteActorCredentialSharingController::IsActive() const {
  return !callback_.is_null();
}

void RemoteActorCredentialSharingController::
    OnGetPasswordStoreResultsOrErrorFrom(
        PasswordStoreInterface* store,
        base::expected<std::vector<StoredCredential>, PasswordStoreBackendError>
            results_or_error) {
  if (!IsActive()) {
    return;
  }

  received_callbacks_++;

  if (results_or_error) {
    std::vector<StoredCredential> logins = std::move(*results_or_error);

    bool is_sync_active =
        sync_service_ &&
        sync_util::IsSyncFeatureActiveIncludingPasswords(sync_service_);

    for (StoredCredential& login : logins) {
      password_manager_util::GetLoginMatchType match_type =
          password_manager_util::GetMatchType(login);
      if (match_type != password_manager_util::GetLoginMatchType::kExact &&
          match_type != password_manager_util::GetLoginMatchType::kAffiliated &&
          match_type != password_manager_util::GetLoginMatchType::kPSL) {
        continue;
      }
      PasswordForm form = ToPasswordForm(std::move(login));
      if (form.IsUsingAccountStore() ||
          (form.IsUsingProfileStore() && is_sync_active)) {
        credentials_.push_back(std::move(form));
      }
    }
  }

  if (received_callbacks_ == expected_callbacks_) {
    OnAllLoginsRetrieved();
  }
}

bool RemoteActorCredentialSharingController::VerifyUserIdentityAndSyncState(
    std::string_view gaia_id) const {
  if (!identity_manager_) {
    return false;
  }
  CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account.gaia.empty() ||
      primary_account.gaia != GaiaId(std::string(gaia_id))) {
    return false;
  }

  if (sync_service_) {
    if (sync_service_->GetAuthError().IsPersistentError()) {
      return false;
    }
    if (sync_service_->GetUserSettings()
            ->IsTrustedVaultKeyRequiredForPreferredDataTypes()) {
      return false;
    }
  }

  return true;
}

void RemoteActorCredentialSharingController::QueryPasswordStores(
    std::string_view domain) {
  const bool is_password_sync_active =
      sync_service_ &&
      sync_util::IsSyncFeatureActiveIncludingPasswords(sync_service_);
  // We only query stores that contain credentials synced to the Google Account.
  // - If password sync is active (sync-the-feature), the profile store contains
  //   the synced credentials.
  // - If sync is inactive but account storage is active (sync-the-transport),
  //   the account store contains the account-scoped credentials.
  // Local-only credentials (profile store when sync is inactive) are excluded
  // from sharing.
  std::vector<PasswordStoreInterface*> stores;
  if (is_password_sync_active) {
    CHECK(profile_store_);
    stores.push_back(profile_store_);
  } else if (sync_service_ &&
             features_util::IsAccountStorageActive(sync_service_)) {
    CHECK(account_store_);
    stores.push_back(account_store_);
  }

  if (stores.empty()) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kNoSyncOrAccountStorage);
    RespondWithError(
        RemoteActorCredentialSharingResult::kNoSyncOrAccountStorage);
    return;
  }

  expected_callbacks_ = stores.size();

  PasswordFormDigest digest(PasswordForm::Scheme::kHtml,
                            base::StrCat({"https://", domain, "/"}),
                            GURL(base::StrCat({"https://", domain})));

  for (PasswordStoreInterface* store : stores) {
    store->GetLogins(digest, weak_ptr_factory_.GetWeakPtr());
  }
}

void RemoteActorCredentialSharingController::OnAllLoginsRetrieved() {
  if (!IsActive()) {
    return;
  }

  if (credentials_.empty()) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kNoPasswordsFound);
    std::move(callback_).Run(
        RemoteActorCredentialSharingResult::kNoPasswordsFound);
    return;
  }

  std::vector<std::unique_ptr<PasswordForm>> credentials_ptr;
  for (PasswordForm& form : credentials_) {
    credentials_ptr.push_back(std::make_unique<PasswordForm>(std::move(form)));
  }

  delegate_->ShowCredentialSelection(
      std::move(credentials_ptr), origin_,
      base::BindOnce(
          &RemoteActorCredentialSharingController::OnCredentialSelected,
          weak_ptr_factory_.GetWeakPtr()));
}

void RemoteActorCredentialSharingController::OnCredentialSelected(
    Delegate::CredentialOrError selected_form) {
  if (!IsActive()) {
    return;
  }

  if (!selected_form.has_value()) {
    LogRemoteActorCredentialSharingResult(selected_form.error());
    RespondWithError(selected_form.error());
    return;
  }

  delegate_->AuthenticateIfNeeded(
      origin_,
      base::BindOnce(
          &RemoteActorCredentialSharingController::ProceedWithCredential,
          weak_ptr_factory_.GetWeakPtr(), std::move(*selected_form)));
}

void RemoteActorCredentialSharingController::ProceedWithCredential(
    PasswordForm selected_form,
    Delegate::AuthResult auth_result) {
  if (!IsActive()) {
    return;
  }

  if (!auth_result.has_value()) {
    LogRemoteActorCredentialSharingResult(auth_result.error());
    RespondWithError(auth_result.error());
    return;
  }

  if (!sharing_service_) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kSharingServiceUnavailable);
    RespondWithError(
        RemoteActorCredentialSharingResult::kSharingServiceUnavailable);
    return;
  }

  StoredCredential credential = FromPasswordForm(std::move(selected_form));
  sync_pb::PasswordSpecificsData specifics_data =
      SpecificsDataFromStoredCredential(credential);

  static constexpr base::TimeDelta kShareTimeToLive = base::Minutes(10);
  RemoteActorCredentialSharingService::ShareParameters params;
  params.obfuscated_gaia_id = gaia_id_;
  params.web_origin = origin_.Serialize();
  params.password_data = std::move(specifics_data);
  params.time_to_live = kShareTimeToLive;
  params.task_id = task_id_;

  sharing_service_->SharePassword(
      params,
      base::BindOnce(&RemoteActorCredentialSharingController::OnShareCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoteActorCredentialSharingController::OnShareCompleted(bool success) {
  RemoteActorCredentialSharingResult result =
      success ? RemoteActorCredentialSharingResult::kSuccess
              : RemoteActorCredentialSharingResult::kSharingFailed;
  LogRemoteActorCredentialSharingResult(result);
  RunResultCallback(result);
}

void RemoteActorCredentialSharingController::RespondWithError(
    RemoteActorCredentialSharingResult result) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoteActorCredentialSharingController::RunResultCallback,
                     weak_ptr_factory_.GetWeakPtr(), result));
}

void RemoteActorCredentialSharingController::RunResultCallback(
    RemoteActorCredentialSharingResult result) {
  std::move(callback_).Run(result);
}

}  // namespace password_manager
