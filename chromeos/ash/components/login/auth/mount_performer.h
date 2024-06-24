// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_MOUNT_PERFORMER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_MOUNT_PERFORMER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"

namespace ash {

class UserContext;

// This class provides higher level API for cryptohomed operations related to
// creating and mounting user home directory.
// This implementation is only compatible with AuthSession-based API.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) MountPerformer {
 public:
  explicit MountPerformer(
      const base::Clock* clock = base::DefaultClock::GetInstance());

  MountPerformer(const MountPerformer&) = delete;
  MountPerformer& operator=(const MountPerformer&) = delete;

  ~MountPerformer();

  // Invalidates any ongoing mount attempts by invalidating Weak pointers on
  // internal callbacks. Callbacks for ongoing operations will not be called
  // afterwards, but there is no guarantee about state of the active mounts.
  void InvalidateCurrentAttempts();

  base::WeakPtr<MountPerformer> AsWeakPtr();

  // Creates persistent home directory for the user identified by auth session
  // in `context`.
  // It is assumed that auth session has `user_exists` set to false.
  // Auth session should *not* be started as ephemeral.
  // Auth session would become authenticated as the result.
  // Does not mount home directory.
  void CreateNewUser(std::unique_ptr<UserContext> context,
                     AuthOperationCallback callback);

  // Mounts persistent directory for the user identified by auth session in
  // `context`.
  // Session should be authenticated.
  // Upon success fills in User id hash in `context`.
  // Would fail if encryption migration is required, in that case
  // `MountForMigration` should be used.
  void MountPersistentDirectory(std::unique_ptr<UserContext> context,
                                AuthOperationCallback callback);

  // Mounts persistent directory for the user identified by auth session in
  // `context` to perform dircrypto migration.
  // Session should be authenticated.
  // Upon success fills in User id hash in `context`.
  void MountForMigration(std::unique_ptr<UserContext> context,
                         AuthOperationCallback callback);

  // Mounts ephemeral directory for the user identified by auth session in
  // `context`.
  // It is assumed that auth session has `user_exists` set to false.
  // Auth session should be started as ephemeral.
  // Auth session would become authenticated as the result.
  // Upon success fills in User id hash in `context`.
  void MountEphemeralDirectory(std::unique_ptr<UserContext> context,
                               AuthOperationCallback callback);

  // Mounts ephemeral directory for the guest user.
  // Upon success fills in User id hash in `context`.
  void MountGuestDirectory(std::unique_ptr<UserContext> context,
                           AuthOperationCallback callback);

  // Removes home directory for the user identified by auth session in
  // `context`.
  // It is expected that home directory is not mounted.
  // Clears AuthSession from `context`.
  void RemoveUserDirectory(std::unique_ptr<UserContext> context,
                           AuthOperationCallback callback);

  // Removes home directory for the user identified by `identifier`
  // It is expected that home directory is not mounted.
  void RemoveUserDirectoryByIdentifier(cryptohome::AccountIdentifier identifier,
                                       NoContextOperationCallback callback);

  // Unmounts all currently mounted directories.
  // Context is required only for passing to callback.
  void UnmountDirectories(std::unique_ptr<UserContext> context,
                          AuthOperationCallback callback);

  void MigrateToDircrypto(std::unique_ptr<UserContext> context,
                          AuthOperationCallback callback);

 private:
  // Callbacks for UserDataAuthClient operations:
  void OnCreatePersistentUser(
      base::Time request_start,
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::CreatePersistentUserReply> reply);
  void OnPrepareGuestVault(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::PrepareGuestVaultReply> reply);
  void OnPrepareEphemeralVault(
      base::Time request_start,
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::PrepareEphemeralVaultReply> reply);
  void OnPreparePersistentVault(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::PreparePersistentVaultReply> reply);
  void OnPrepareVaultForMigration(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::PrepareVaultForMigrationReply> reply);
  void OnServiceRunning(cryptohome::AccountIdentifier identifier,
                        NoContextOperationCallback callback,
                        bool service_is_available);
  void OnRemove(std::unique_ptr<UserContext> context,
                AuthOperationCallback callback,
                std::optional<user_data_auth::RemoveReply> reply);
  void OnRemoveByIdentifier(NoContextOperationCallback callback,
                            std::optional<user_data_auth::RemoveReply> reply);
  void OnUnmount(std::unique_ptr<UserContext> context,
                 AuthOperationCallback callback,
                 std::optional<user_data_auth::UnmountReply> reply);
  void OnMigrateToDircrypto(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::StartMigrateToDircryptoReply> reply);

  const raw_ptr<const base::Clock> clock_;
  base::WeakPtrFactory<MountPerformer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_MOUNT_PERFORMER_H_
