// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/userdataauth/fake_userdataauth_client.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"

namespace chromeos {

namespace {

// Interval to update the progress of MigrateToDircrypto in milliseconds.
constexpr int kDircryptoMigrationUpdateIntervalMs = 200;
// The number of updates the MigrateToDircrypto will send before it completes.
constexpr uint64_t kDircryptoMigrationMaxProgress = 15;

// Template for auth session ID.
constexpr char kAuthSessionIdTemplate[] = "AuthSession-%d";

// Guest username constant that mirrors the one in real cryptohome
constexpr char kGuestUserName[] = "$guest";

// Used to track the fake instance, mirrors the instance in the base class.
FakeUserDataAuthClient* g_instance = nullptr;

}  // namespace

FakeUserDataAuthClient::FakeUserDataAuthClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeUserDataAuthClient::~FakeUserDataAuthClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeUserDataAuthClient* FakeUserDataAuthClient::Get() {
  return g_instance;
}

void FakeUserDataAuthClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeUserDataAuthClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeUserDataAuthClient::IsMounted(
    const ::user_data_auth::IsMountedRequest& request,
    IsMountedCallback callback) {
  ::user_data_auth::IsMountedReply reply;
  reply.set_is_mounted(true);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::Unmount(
    const ::user_data_auth::UnmountRequest& request,
    UnmountCallback callback) {
  ReturnProtobufMethodCallback(::user_data_auth::UnmountReply(),
                               std::move(callback));
}
void FakeUserDataAuthClient::Mount(
    const ::user_data_auth::MountRequest& request,
    MountCallback callback) {
  ::user_data_auth::CryptohomeErrorCode error = cryptohome_error_;
  last_mount_request_ = request;
  ::user_data_auth::MountReply reply;

  cryptohome::AccountIdentifier account;
  if (request.guest_mount()) {
    account.set_account_id(kGuestUserName);
    reply.set_sanitized_username(GetStubSanitizedUsername(account));
  } else {
    if (request.has_account()) {
      account = request.account();
      reply.set_sanitized_username(GetStubSanitizedUsername(account));
      if (mount_create_required_ && !request.has_create())
        error = ::user_data_auth::CryptohomeErrorCode::
            CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND;
    } else {
      auto auth_session = auth_sessions_.find(request.auth_session_id());
      DCHECK(auth_session != std::end(auth_sessions_));
      account = auth_session->second.account;
    }

    reply.set_sanitized_username(GetStubSanitizedUsername(account));
    if (IsEcryptfsUserHome(account) && !request.to_migrate_from_ecryptfs() &&
        request.force_dircrypto_if_available()) {
      error = ::user_data_auth::CryptohomeErrorCode::
          CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION;
    }
  }

  reply.set_error(error);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::Remove(
    const ::user_data_auth::RemoveRequest& request,
    RemoveCallback callback) {
  ReturnProtobufMethodCallback(::user_data_auth::RemoveReply(),
                               std::move(callback));
}
void FakeUserDataAuthClient::GetKeyData(
    const ::user_data_auth::GetKeyDataRequest& request,
    GetKeyDataCallback callback) {
  ::user_data_auth::GetKeyDataReply reply;
  const auto it = key_data_map_.find(request.account_id());
  if (it == key_data_map_.end()) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
  } else if (it->second.empty()) {
    reply.set_error(
        ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
  } else {
    auto key = FindKey(it->second,
                       request.authorization_request().key().data().label());
    if (key != it->second.end()) {
      *reply.add_key_data() = key->second.data();
    } else {
      reply.set_error(::user_data_auth::CryptohomeErrorCode::
                          CRYPTOHOME_ERROR_KEY_NOT_FOUND);
    }
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::CheckKey(
    const ::user_data_auth::CheckKeyRequest& request,
    CheckKeyCallback callback) {
  ::user_data_auth::CheckKeyReply reply;

  if (enable_auth_check_) {
    const auto it = key_data_map_.find(request.account_id());
    if (it == key_data_map_.end()) {
      reply.set_error(::user_data_auth::CryptohomeErrorCode::
                          CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    } else if (it->second.empty()) {
      reply.set_error(::user_data_auth::CryptohomeErrorCode::
                          CRYPTOHOME_ERROR_KEY_NOT_FOUND);
    } else {
      auto key = FindKey(it->second,
                         request.authorization_request().key().data().label());
      if (key == it->second.end()) {
        reply.set_error(::user_data_auth::CryptohomeErrorCode::
                            CRYPTOHOME_ERROR_KEY_NOT_FOUND);
      } else if (key->second.secret() !=
                 request.authorization_request().key().secret()) {
        reply.set_error(::user_data_auth::CryptohomeErrorCode::
                            CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
      }
    }
  }

  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::AddKey(
    const ::user_data_auth::AddKeyRequest& request,
    AddKeyCallback callback) {
  key_data_map_[request.account_id()][request.key().data().label()] =
      request.key();
  ReturnProtobufMethodCallback(::user_data_auth::AddKeyReply(),
                               std::move(callback));
}
void FakeUserDataAuthClient::RemoveKey(
    const ::user_data_auth::RemoveKeyRequest& request,
    RemoveKeyCallback callback) {
  const auto it = key_data_map_.find(request.account_id());
  if (it != key_data_map_.end()) {
    auto key = FindKey(it->second, request.key().data().label());
    if (key != it->second.end())
      it->second.erase(key);
  }
  ReturnProtobufMethodCallback(::user_data_auth::RemoveKeyReply(),
                               std::move(callback));
}
void FakeUserDataAuthClient::MassRemoveKeys(
    const ::user_data_auth::MassRemoveKeysRequest& request,
    MassRemoveKeysCallback callback) {
  ReturnProtobufMethodCallback(::user_data_auth::MassRemoveKeysReply(),
                               std::move(callback));
}
void FakeUserDataAuthClient::MigrateKey(
    const ::user_data_auth::MigrateKeyRequest& request,
    MigrateKeyCallback callback) {
  ReturnProtobufMethodCallback(::user_data_auth::MigrateKeyReply(),
                               std::move(callback));
}
void FakeUserDataAuthClient::StartFingerprintAuthSession(
    const ::user_data_auth::StartFingerprintAuthSessionRequest& request,
    StartFingerprintAuthSessionCallback callback) {
  ReturnProtobufMethodCallback(
      ::user_data_auth::StartFingerprintAuthSessionReply(),
      std::move(callback));
}
void FakeUserDataAuthClient::EndFingerprintAuthSession(
    const ::user_data_auth::EndFingerprintAuthSessionRequest& request,
    EndFingerprintAuthSessionCallback callback) {
  ReturnProtobufMethodCallback(
      ::user_data_auth::EndFingerprintAuthSessionReply(), std::move(callback));
}
void FakeUserDataAuthClient::StartMigrateToDircrypto(
    const ::user_data_auth::StartMigrateToDircryptoRequest& request,
    StartMigrateToDircryptoCallback callback) {
  last_migrate_to_dircrypto_request_ = request;
  ReturnProtobufMethodCallback(::user_data_auth::StartMigrateToDircryptoReply(),
                               std::move(callback));

  dircrypto_migration_progress_ = 0;
  if (run_default_dircrypto_migration_) {
    dircrypto_migration_progress_timer_.Start(
        FROM_HERE, base::Milliseconds(kDircryptoMigrationUpdateIntervalMs),
        this, &FakeUserDataAuthClient::OnDircryptoMigrationProgressUpdated);
  }
}
void FakeUserDataAuthClient::NeedsDircryptoMigration(
    const ::user_data_auth::NeedsDircryptoMigrationRequest& request,
    NeedsDircryptoMigrationCallback callback) {
  ::user_data_auth::NeedsDircryptoMigrationReply reply;
  reply.set_needs_dircrypto_migration(IsEcryptfsUserHome(request.account_id()));
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::GetSupportedKeyPolicies(
    const ::user_data_auth::GetSupportedKeyPoliciesRequest& request,
    GetSupportedKeyPoliciesCallback callback) {
  ::user_data_auth::GetSupportedKeyPoliciesReply reply;
  reply.set_low_entropy_credentials_supported(
      supports_low_entropy_credentials_);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::GetAccountDiskUsage(
    const ::user_data_auth::GetAccountDiskUsageRequest& request,
    GetAccountDiskUsageCallback callback) {
  ::user_data_auth::GetAccountDiskUsageReply reply;
  // Sets 100 MB as a fake usage.
  reply.set_size(100 * 1024 * 1024);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::StartAuthSession(
    const ::user_data_auth::StartAuthSessionRequest& request,
    StartAuthSessionCallback callback) {
  std::string auth_session_id =
      base::StringPrintf(kAuthSessionIdTemplate, next_auth_session_id_++);

  DCHECK_EQ(auth_sessions_.count(auth_session_id), 0u);
  AuthSessionData& session = auth_sessions_[auth_session_id];
  session.id = auth_session_id;
  session.account = request.account_id();

  ::user_data_auth::StartAuthSessionReply reply;
  reply.set_auth_session_id(auth_session_id);

  reply.set_user_exists(UserExists(request.account_id()));

  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::AuthenticateAuthSession(
    const ::user_data_auth::AuthenticateAuthSessionRequest& request,
    AuthenticateAuthSessionCallback callback) {
  last_authenticate_auth_session_request_ = request;
  ::user_data_auth::AuthenticateAuthSessionReply reply;

  const std::string auth_session_id = request.auth_session_id();

  const auto it = auth_sessions_.find(auth_session_id);
  if (it == auth_sessions_.end()) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
  } else {
    it->second.authenticated = true;
    reply.set_authenticated(true);
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::AddCredentials(
    const ::user_data_auth::AddCredentialsRequest& request,
    AddCredentialsCallback callback) {
  ::user_data_auth::AddCredentialsReply reply;

  const std::string auth_session_id = request.auth_session_id();

  const auto it = auth_sessions_.find(auth_session_id);
  if (it == auth_sessions_.end()) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  if (service_is_available_ || service_reported_not_available_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), service_is_available_));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeUserDataAuthClient::SetEcryptfsUserHome(
    const cryptohome::AccountIdentifier& cryptohome_id,
    bool use_ecryptfs) {
  if (use_ecryptfs)
    ecryptfs_user_homes_.insert(cryptohome_id);
  else
    ecryptfs_user_homes_.erase(cryptohome_id);
}

void FakeUserDataAuthClient::SetServiceIsAvailable(bool is_available) {
  service_is_available_ = is_available;
  if (!is_available)
    return;

  std::vector<WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(true);
}

void FakeUserDataAuthClient::ReportServiceIsNotAvailable() {
  DCHECK(!service_is_available_);
  service_reported_not_available_ = true;

  std::vector<WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(false);
}

template <typename ReplyType>
void FakeUserDataAuthClient::ReturnProtobufMethodCallback(
    const ReplyType& reply,
    DBusMethodCallback<ReplyType> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

void FakeUserDataAuthClient::OnDircryptoMigrationProgressUpdated() {
  dircrypto_migration_progress_++;

  if (dircrypto_migration_progress_ >= kDircryptoMigrationMaxProgress) {
    NotifyDircryptoMigrationProgress(
        ::user_data_auth::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
        dircrypto_migration_progress_, kDircryptoMigrationMaxProgress);
    SetEcryptfsUserHome(last_migrate_to_dircrypto_request_.account_id(), false);
    dircrypto_migration_progress_timer_.Stop();
    return;
  }
  NotifyDircryptoMigrationProgress(::user_data_auth::DircryptoMigrationStatus::
                                       DIRCRYPTO_MIGRATION_IN_PROGRESS,
                                   dircrypto_migration_progress_,
                                   kDircryptoMigrationMaxProgress);
}

void FakeUserDataAuthClient::NotifyLowDiskSpace(uint64_t disk_free_bytes) {
  ::user_data_auth::LowDiskSpace status;
  status.set_disk_free_bytes(disk_free_bytes);
  for (auto& observer : observer_list_)
    observer.LowDiskSpace(status);
}

void FakeUserDataAuthClient::NotifyDircryptoMigrationProgress(
    ::user_data_auth::DircryptoMigrationStatus status,
    uint64_t current,
    uint64_t total) {
  ::user_data_auth::DircryptoMigrationProgress progress;
  progress.set_status(status);
  progress.set_current_bytes(current);
  progress.set_total_bytes(total);
  for (auto& observer : observer_list_)
    observer.DircryptoMigrationProgress(progress);
}

bool FakeUserDataAuthClient::IsEcryptfsUserHome(
    const cryptohome::AccountIdentifier& cryptohome_id) {
  return base::Contains(ecryptfs_user_homes_, cryptohome_id);
}

std::map<std::string, cryptohome::Key>::const_iterator
FakeUserDataAuthClient::FindKey(
    const std::map<std::string, cryptohome::Key>& keys,
    const std::string& label) {
  // Wildcard label.
  if (label.empty())
    return keys.begin();

  // Specific label
  return keys.find(label);
}

bool FakeUserDataAuthClient::UserExists(
    const cryptohome::AccountIdentifier& account_id) const {
  return existing_users_.find(account_id) != std::end(existing_users_);
}

void FakeUserDataAuthClient::AddExistingUser(
    const cryptohome::AccountIdentifier& account_id) {
  existing_users_.insert(account_id);
}

}  // namespace chromeos
