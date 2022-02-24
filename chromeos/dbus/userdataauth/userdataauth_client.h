// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// UserDataAuthClient is used to communicate with the org.chromium.UserDataAuth
// service exposed by cryptohomed. All method should be called from the origin
// thread (UI thread) which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) UserDataAuthClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when LowDiskSpace signal is received, when the cryptohome
    // partition is running out of disk space.
    virtual void LowDiskSpace(const ::user_data_auth::LowDiskSpace& status) {}

    // Called when DircryptoMigrationProgress signal is received.
    // Typically, called periodically during a migration is performed by
    // cryptohomed, as well as to notify the completion of migration.
    virtual void DircryptoMigrationProgress(
        const ::user_data_auth::DircryptoMigrationProgress& progress) {}
  };

  using IsMountedCallback =
      DBusMethodCallback<::user_data_auth::IsMountedReply>;
  using UnmountCallback = DBusMethodCallback<::user_data_auth::UnmountReply>;
  using MountCallback = DBusMethodCallback<::user_data_auth::MountReply>;
  using RemoveCallback = DBusMethodCallback<::user_data_auth::RemoveReply>;
  using GetKeyDataCallback =
      DBusMethodCallback<::user_data_auth::GetKeyDataReply>;
  using CheckKeyCallback = DBusMethodCallback<::user_data_auth::CheckKeyReply>;
  using AddKeyCallback = DBusMethodCallback<::user_data_auth::AddKeyReply>;
  using RemoveKeyCallback =
      DBusMethodCallback<::user_data_auth::RemoveKeyReply>;
  using MassRemoveKeysCallback =
      DBusMethodCallback<::user_data_auth::MassRemoveKeysReply>;
  using MigrateKeyCallback =
      DBusMethodCallback<::user_data_auth::MigrateKeyReply>;
  using StartFingerprintAuthSessionCallback =
      DBusMethodCallback<::user_data_auth::StartFingerprintAuthSessionReply>;
  using EndFingerprintAuthSessionCallback =
      DBusMethodCallback<::user_data_auth::EndFingerprintAuthSessionReply>;
  using StartMigrateToDircryptoCallback =
      DBusMethodCallback<::user_data_auth::StartMigrateToDircryptoReply>;
  using NeedsDircryptoMigrationCallback =
      DBusMethodCallback<::user_data_auth::NeedsDircryptoMigrationReply>;
  using GetSupportedKeyPoliciesCallback =
      DBusMethodCallback<::user_data_auth::GetSupportedKeyPoliciesReply>;
  using GetAccountDiskUsageCallback =
      DBusMethodCallback<::user_data_auth::GetAccountDiskUsageReply>;
  using StartAuthSessionCallback =
      DBusMethodCallback<::user_data_auth::StartAuthSessionReply>;
  using AuthenticateAuthSessionCallback =
      DBusMethodCallback<::user_data_auth::AuthenticateAuthSessionReply>;
  using AddCredentialsCallback =
      DBusMethodCallback<::user_data_auth::AddCredentialsReply>;
  using PrepareGuestVaultCallback =
      DBusMethodCallback<::user_data_auth::PrepareGuestVaultReply>;
  using PrepareEphemeralVaultCallback =
      DBusMethodCallback<::user_data_auth::PrepareEphemeralVaultReply>;
  using CreatePersistentUserCallback =
      DBusMethodCallback<::user_data_auth::CreatePersistentUserReply>;
  using PreparePersistentVaultCallback =
      DBusMethodCallback<::user_data_auth::PreparePersistentVaultReply>;
  using InvalidateAuthSessionCallback =
      DBusMethodCallback<::user_data_auth::InvalidateAuthSessionReply>;
  using ExtendAuthSessionCallback =
      DBusMethodCallback<::user_data_auth::ExtendAuthSessionReply>;
  using AddAuthFactorCallback =
      DBusMethodCallback<::user_data_auth::AddAuthFactorReply>;
  using AuthenticateAuthFactorCallback =
      DBusMethodCallback<::user_data_auth::AuthenticateAuthFactorReply>;
  using UpdateAuthFactorCallback =
      DBusMethodCallback<::user_data_auth::UpdateAuthFactorReply>;
  using RemoveAuthFactorCallback =
      DBusMethodCallback<::user_data_auth::RemoveAuthFactorReply>;

  // Not copyable or movable.
  UserDataAuthClient(const UserDataAuthClient&) = delete;
  UserDataAuthClient& operator=(const UserDataAuthClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static UserDataAuthClient* Get();

  // Returns the sanitized |username| that the stub implementation would return.
  static std::string GetStubSanitizedUsername(
      const cryptohome::AccountIdentifier& id);

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Actual DBus Methods:

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) = 0;

  // Queries if user's vault is mounted.
  virtual void IsMounted(const ::user_data_auth::IsMountedRequest& request,
                         IsMountedCallback callback) = 0;

  // Unmounts user's vault.
  virtual void Unmount(const ::user_data_auth::UnmountRequest& request,
                       UnmountCallback callback) = 0;

  // Mounts user's vault.
  virtual void Mount(const ::user_data_auth::MountRequest& request,
                     MountCallback callback) = 0;

  // Removes user's vault.
  virtual void Remove(const ::user_data_auth::RemoveRequest& request,
                      RemoveCallback callback) = 0;

  // Get key metadata for user's vault.
  virtual void GetKeyData(const ::user_data_auth::GetKeyDataRequest& request,
                          GetKeyDataCallback callback) = 0;

  // Try authenticating with key in user's vault.
  virtual void CheckKey(const ::user_data_auth::CheckKeyRequest& request,
                        CheckKeyCallback callback) = 0;

  // Add a key to user's vault.
  virtual void AddKey(const ::user_data_auth::AddKeyRequest& request,
                      AddKeyCallback callback) = 0;

  // Remove a key from user's vault.
  virtual void RemoveKey(const ::user_data_auth::RemoveKeyRequest& request,
                         RemoveKeyCallback callback) = 0;

  // Remove multiple keys from user's vault.
  virtual void MassRemoveKeys(
      const ::user_data_auth::MassRemoveKeysRequest& request,
      MassRemoveKeysCallback callback) = 0;

  // Change the user vault's key's authentication.
  virtual void MigrateKey(const ::user_data_auth::MigrateKeyRequest& request,
                          MigrateKeyCallback callback) = 0;

  // Starts a fingerprint auth session.
  virtual void StartFingerprintAuthSession(
      const ::user_data_auth::StartFingerprintAuthSessionRequest& request,
      StartFingerprintAuthSessionCallback callback) = 0;

  // Ends a fingerprint auth session.
  virtual void EndFingerprintAuthSession(
      const ::user_data_auth::EndFingerprintAuthSessionRequest& request,
      EndFingerprintAuthSessionCallback callback) = 0;

  // Instructs cryptohome to migrate the vault from eCryptfs to Dircrypto.
  virtual void StartMigrateToDircrypto(
      const ::user_data_auth::StartMigrateToDircryptoRequest& request,
      StartMigrateToDircryptoCallback callback) = 0;

  // Check with cryptohome to see if a user's vault needs to be migrated.
  virtual void NeedsDircryptoMigration(
      const ::user_data_auth::NeedsDircryptoMigrationRequest& request,
      NeedsDircryptoMigrationCallback callback) = 0;

  // Check the capabilities/policies regarding a key. For instance, if low
  // entropy credential is supported.
  virtual void GetSupportedKeyPolicies(
      const ::user_data_auth::GetSupportedKeyPoliciesRequest& request,
      GetSupportedKeyPoliciesCallback callback) = 0;

  // Calculate the amount of disk space used by user's vault.
  virtual void GetAccountDiskUsage(
      const ::user_data_auth::GetAccountDiskUsageRequest& request,
      GetAccountDiskUsageCallback callback) = 0;

  // Starts an auth session.
  virtual void StartAuthSession(
      const ::user_data_auth::StartAuthSessionRequest& request,
      StartAuthSessionCallback callback) = 0;

  // Attempts to authenticate with the given auth session.
  virtual void AuthenticateAuthSession(
      const ::user_data_auth::AuthenticateAuthSessionRequest& request,
      AuthenticateAuthSessionCallback callback) = 0;

  // Attempts to add credentials to the vault identified/authorized by auth
  // session.
  virtual void AddCredentials(
      const ::user_data_auth::AddCredentialsRequest& request,
      AddCredentialsCallback callback) = 0;

  // This request is intended to happen when a user wants
  // to login to ChromeOS as a guest.
  virtual void PrepareGuestVault(
      const ::user_data_auth::PrepareGuestVaultRequest& request,
      PrepareGuestVaultCallback callback) = 0;

  // This request is intended when a policy (either device or enterprise)
  // has enabled ephemeral users. An ephemeral user is created
  // in a memory filesystem only and is never actually persisted to disk.
  virtual void PrepareEphemeralVault(
      const ::user_data_auth::PrepareEphemeralVaultRequest& request,
      PrepareEphemeralVaultCallback callback) = 0;

  // This will create user directories needed to store
  // keys and download policies. This will usually be called when a new user is
  // registering.
  virtual void CreatePersistentUser(
      const ::user_data_auth::CreatePersistentUserRequest& request,
      CreatePersistentUserCallback callback) = 0;

  // This makes available user directories for them to use.
  virtual void PreparePersistentVault(
      const ::user_data_auth::PreparePersistentVaultRequest& request,
      PreparePersistentVaultCallback callback) = 0;

  // This call is used to invalidate an AuthSession
  // once the need for one no longer exists.
  virtual void InvalidateAuthSession(
      const ::user_data_auth::InvalidateAuthSessionRequest& request,
      InvalidateAuthSessionCallback callback) = 0;

  // This call is used to extend the duration of
  //  AuthSession that it should be valid for.
  virtual void ExtendAuthSession(
      const ::user_data_auth::ExtendAuthSessionRequest& request,
      ExtendAuthSessionCallback callback) = 0;

  // This call adds an AuthFactor for a user. The call goes
  // through an authenticated AuthSession.
  virtual void AddAuthFactor(
      const ::user_data_auth::AddAuthFactorRequest& request,
      AddAuthFactorCallback callback) = 0;

  // This will Authenticate an existing AuthFactor.
  // This call will authenticate an AuthSession.
  virtual void AuthenticateAuthFactor(
      const ::user_data_auth::AuthenticateAuthFactorRequest& request,
      AuthenticateAuthFactorCallback callback) = 0;

  // This call will be used in the case of a user wanting
  // to update an AuthFactor. (E.g. Changing pin or password).
  virtual void UpdateAuthFactor(
      const ::user_data_auth::UpdateAuthFactorRequest& request,
      UpdateAuthFactorCallback callback) = 0;

  // This is called when a user wants to remove an
  // AuthFactor.
  virtual void RemoveAuthFactor(
      const ::user_data_auth::RemoveAuthFactorRequest& request,
      RemoveAuthFactorCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  UserDataAuthClient();
  virtual ~UserDataAuthClient();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::UserDataAuthClient;
}

#endif  // CHROMEOS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_
