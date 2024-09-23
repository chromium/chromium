// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/mount_performer.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/cryptohome/error_types.h"
#include "chromeos/ash/components/cryptohome/error_util.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

namespace {

bool ShouldUseOldEncryptionForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kCryptohomeUseOldEncryptionForTesting);
}

}  // namespace

MountPerformer::MountPerformer(const base::Clock* clock) : clock_(clock) {}
MountPerformer::~MountPerformer() = default;

void MountPerformer::InvalidateCurrentAttempts() {
  weak_factory_.InvalidateWeakPtrs();
}

base::WeakPtr<MountPerformer> MountPerformer::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MountPerformer::CreateNewUser(std::unique_ptr<UserContext> context,
                                   AuthOperationCallback callback) {
  user_data_auth::CreatePersistentUserRequest request;
  LOGIN_LOG(EVENT) << "Create persistent directory";
  request.set_auth_session_id(context->GetAuthSessionId());
  UserDataAuthClient::Get()->CreatePersistentUser(
      request, base::BindOnce(&MountPerformer::OnCreatePersistentUser,
                              weak_factory_.GetWeakPtr(), clock_->Now(),
                              std::move(context), std::move(callback)));
}

void MountPerformer::MountPersistentDirectory(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  user_data_auth::PreparePersistentVaultRequest request;
  LOGIN_LOG(EVENT) << "Mount persistent directory";
  request.set_auth_session_id(context->GetAuthSessionId());
  if (ShouldUseOldEncryptionForTesting()) {
    request.set_encryption_type(
        user_data_auth::CRYPTOHOME_VAULT_ENCRYPTION_ECRYPTFS);
  } else if (context->IsForcingDircrypto()) {
    request.set_block_ecryptfs(true);
  }
  UserDataAuthClient::Get()->PreparePersistentVault(
      request, base::BindOnce(&MountPerformer::OnPreparePersistentVault,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void MountPerformer::MountForMigration(std::unique_ptr<UserContext> context,
                                       AuthOperationCallback callback) {
  user_data_auth::PrepareVaultForMigrationRequest request;
  LOGIN_LOG(EVENT) << "Mount persistent directory for migration";
  request.set_auth_session_id(context->GetAuthSessionId());
  UserDataAuthClient::Get()->PrepareVaultForMigration(
      request, base::BindOnce(&MountPerformer::OnPrepareVaultForMigration,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void MountPerformer::MountEphemeralDirectory(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  user_data_auth::PrepareEphemeralVaultRequest request;
  LOGIN_LOG(EVENT) << "Mount ephemeral directory";
  request.set_auth_session_id(context->GetAuthSessionId());
  UserDataAuthClient::Get()->PrepareEphemeralVault(
      request, base::BindOnce(&MountPerformer::OnPrepareEphemeralVault,
                              weak_factory_.GetWeakPtr(), clock_->Now(),
                              std::move(context), std::move(callback)));
}

void MountPerformer::MountGuestDirectory(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  user_data_auth::PrepareGuestVaultRequest request;
  LOGIN_LOG(EVENT) << "Mount guest directory";
  UserDataAuthClient::Get()->PrepareGuestVault(
      request, base::BindOnce(&MountPerformer::OnPrepareGuestVault,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void MountPerformer::RemoveUserDirectory(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  user_data_auth::RemoveRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());
  LOGIN_LOG(EVENT) << "Removing user directory";
  UserDataAuthClient::Get()->Remove(
      request,
      base::BindOnce(&MountPerformer::OnRemove, weak_factory_.GetWeakPtr(),
                     std::move(context), std::move(callback)));
}

void MountPerformer::RemoveUserDirectoryByIdentifier(
    cryptohome::AccountIdentifier identifier,
    NoContextOperationCallback callback) {
  LOGIN_LOG(EVENT) << "Checking for crypthomed's availability before "
                      "attempting to remove the user using their identifier";
  UserDataAuthClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &MountPerformer::OnServiceRunning, weak_factory_.GetWeakPtr(), identifier,
      std::move(callback)));
}

void MountPerformer::OnServiceRunning(cryptohome::AccountIdentifier identifier,
                                      NoContextOperationCallback callback,
                                      bool service_is_available) {
  if (!service_is_available) {
    LOGIN_LOG(ERROR)
        << "Unable to initiate user removal, service is not available";
    std::move(callback).Run(std::nullopt);
    return;
  }
  user_data_auth::RemoveRequest request;
  *request.mutable_identifier() = identifier;

  LOGIN_LOG(EVENT) << "Removing user directory by cryptohome identifier";
  UserDataAuthClient::Get()->Remove(
      request, base::BindOnce(&MountPerformer::OnRemoveByIdentifier,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MountPerformer::OnRemoveByIdentifier(
    NoContextOperationCallback callback,
    std::optional<user_data_auth::RemoveReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    std::move(callback).Run(AuthenticationError{error});
    return;
  }
  std::move(callback).Run(std::nullopt);
}

// Unmounts all currently mounted directories.
void MountPerformer::UnmountDirectories(std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback) {
  user_data_auth::UnmountRequest request;
  LOGIN_LOG(EVENT) << "Unmounting all directories";
  UserDataAuthClient::Get()->Unmount(
      request,
      base::BindOnce(&MountPerformer::OnUnmount, weak_factory_.GetWeakPtr(),
                     std::move(context), std::move(callback)));
}

void MountPerformer::MigrateToDircrypto(std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback) {
  user_data_auth::StartMigrateToDircryptoRequest request;
  LOGIN_LOG(EVENT) << "Starting dircrypto migration";
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context->GetAccountId());
  UserDataAuthClient::Get()->StartMigrateToDircrypto(
      request, base::BindOnce(&MountPerformer::OnMigrateToDircrypto,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

/// ---- private callbacks ----

void MountPerformer::OnCreatePersistentUser(
    base::Time request_start,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::CreatePersistentUserReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "CreatePersistentUser failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  CHECK(reply->has_auth_properties());
  AuthPerformer::FillAuthenticationData(request_start, reply->auth_properties(),
                                        *context);
  context->SetMountState(UserContext::MountState::kNewPersistent);
  std::move(callback).Run(std::move(context), std::nullopt);
}

void MountPerformer::OnPrepareGuestVault(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::PrepareGuestVaultReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  bool is_success = !cryptohome::HasError(error);
  AuthEventsRecorder::Get()->OnUserVaultPrepared(
      AuthEventsRecorder::UserVaultType::kGuest, is_success);
  if (!is_success) {
    LOGIN_LOG(ERROR) << "PrepareGuestVault failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  context->SetMountState(UserContext::MountState::kEphemeral);
  context->SetUserIDHash(reply->sanitized_username());
  std::move(callback).Run(std::move(context), std::nullopt);
}

void MountPerformer::OnPrepareEphemeralVault(
    base::Time request_start,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::PrepareEphemeralVaultReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  bool is_success = !cryptohome::HasError(error);
  AuthEventsRecorder::Get()->OnUserVaultPrepared(
      AuthEventsRecorder::UserVaultType::kEphemeral, is_success);
  if (!is_success) {
    LOGIN_LOG(ERROR) << "PrepareEphemeralVault failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  CHECK(reply->has_auth_properties());
  AuthPerformer::FillAuthenticationData(request_start, reply->auth_properties(),
                                        *context);
  context->SetMountState(UserContext::MountState::kEphemeral);
  context->SetUserIDHash(reply->sanitized_username());
  std::move(callback).Run(std::move(context), std::nullopt);
}

void MountPerformer::OnPreparePersistentVault(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::PreparePersistentVaultReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  bool is_success = !cryptohome::HasError(error);
  AuthEventsRecorder::Get()->OnUserVaultPrepared(
      AuthEventsRecorder::UserVaultType::kPersistent, is_success);
  if (!is_success) {
    LOGIN_LOG(ERROR) << "PreparePersistentVault failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  if (!context->GetMountState()) {
    context->SetMountState(UserContext::MountState::kExistingPersistent);
  }
  context->SetUserIDHash(reply->sanitized_username());
  std::move(callback).Run(std::move(context), std::nullopt);
}

void MountPerformer::OnPrepareVaultForMigration(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::PrepareVaultForMigrationReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  bool is_success = !cryptohome::HasError(error);
  AuthEventsRecorder::Get()->OnUserVaultPrepared(
      AuthEventsRecorder::UserVaultType::kPersistent, is_success);
  if (!is_success) {
    LOGIN_LOG(ERROR) << "PrepareVaultForMigration failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  CHECK(!context->GetMountState());
  context->SetMountState(UserContext::MountState::kExistingPersistent);
  context->SetUserIDHash(reply->sanitized_username());
  std::move(callback).Run(std::move(context), std::nullopt);
}

void MountPerformer::OnRemove(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::RemoveReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "Remove failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  context->ResetAuthSessionIds();
  std::move(callback).Run(std::move(context), std::nullopt);
}

void MountPerformer::OnUnmount(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::UnmountReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "Unmount failed with error" << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  std::move(callback).Run(std::move(context), std::nullopt);
}

void MountPerformer::OnMigrateToDircrypto(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::StartMigrateToDircryptoReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "MigrateToDircrypto failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  std::move(callback).Run(std::move(context), std::nullopt);
}

}  // namespace ash
