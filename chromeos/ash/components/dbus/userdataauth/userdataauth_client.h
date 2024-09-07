// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace dbus {
class Bus;
}

namespace ash {

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

  class FingerprintAuthObserver : public base::CheckedObserver {
   public:
    // Used for the legacy fingerprint auth scan signal.
    virtual void OnFingerprintScan(
        const ::user_data_auth::FingerprintScanResult& result) {}
    // Used for the legacy fingerprint enroll scan signal.
    virtual void OnEnrollScanDone(
        const ::user_data_auth::FingerprintScanResult& result,
        bool is_complete,
        int percent_complete) {}
  };

  // Processes sub messages embedded in the PrepareAuthFactorProgress signal
  // received
  class PrepareAuthFactorProgressObserver : public base::CheckedObserver {
   public:
    // Called when a fingerprint auth message is received.
    virtual void OnFingerprintAuthScan(
        const ::user_data_auth::AuthScanDone& result) {}

    // Called when a enroll progress is received.
    virtual void OnFingerprintEnrollProgress(
        const ::user_data_auth::AuthEnrollmentProgress& result) {}
  };

  class AuthFactorStatusUpdateObserver : public base::CheckedObserver {
   public:
    // Called when AuthFactorStatusUpdate signal is received.
    virtual void OnAuthFactorStatusUpdate(
        const ::user_data_auth::AuthFactorStatusUpdate& update) {}
  };

  using IsMountedCallback =
      chromeos::DBusMethodCallback<::user_data_auth::IsMountedReply>;
  using GetVaultPropertiesCallback =
      chromeos::DBusMethodCallback<::user_data_auth::GetVaultPropertiesReply>;
  using UnmountCallback =
      chromeos::DBusMethodCallback<::user_data_auth::UnmountReply>;
  using RemoveCallback =
      chromeos::DBusMethodCallback<::user_data_auth::RemoveReply>;

  using GetSupportedKeyPoliciesCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::GetSupportedKeyPoliciesReply>;
  using GetAccountDiskUsageCallback =
      chromeos::DBusMethodCallback<::user_data_auth::GetAccountDiskUsageReply>;

  // AuthSession interaction API.
  using StartAuthSessionCallback =
      chromeos::DBusMethodCallback<::user_data_auth::StartAuthSessionReply>;
  using GetAuthSessionStatusCallback =
      chromeos::DBusMethodCallback<::user_data_auth::GetAuthSessionStatusReply>;
  using InvalidateAuthSessionCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::InvalidateAuthSessionReply>;
  using ExtendAuthSessionCallback =
      chromeos::DBusMethodCallback<::user_data_auth::ExtendAuthSessionReply>;
  // AuthFactors API for AuthSession.
  using AuthenticateAuthFactorCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::AuthenticateAuthFactorReply>;
  using AddAuthFactorCallback =
      chromeos::DBusMethodCallback<::user_data_auth::AddAuthFactorReply>;
  using UpdateAuthFactorCallback =
      chromeos::DBusMethodCallback<::user_data_auth::UpdateAuthFactorReply>;
  using UpdateAuthFactorMetadataCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::UpdateAuthFactorMetadataReply>;
  using ReplaceAuthFactorCallback =
      chromeos::DBusMethodCallback<::user_data_auth::ReplaceAuthFactorReply>;
  using RemoveAuthFactorCallback =
      chromeos::DBusMethodCallback<::user_data_auth::RemoveAuthFactorReply>;
  using ListAuthFactorsCallback =
      chromeos::DBusMethodCallback<::user_data_auth::ListAuthFactorsReply>;
  using GetAuthFactorExtendedInfoCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::GetAuthFactorExtendedInfoReply>;

  // Asynchronous (biometric) AuthFactors API.
  using PrepareAuthFactorCallback =
      chromeos::DBusMethodCallback<::user_data_auth::PrepareAuthFactorReply>;
  using TerminateAuthFactorCallback =
      chromeos::DBusMethodCallback<::user_data_auth::TerminateAuthFactorReply>;
  // Home directory-related API.
  using PrepareGuestVaultCallback =
      chromeos::DBusMethodCallback<::user_data_auth::PrepareGuestVaultReply>;
  using PrepareEphemeralVaultCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::PrepareEphemeralVaultReply>;
  using CreatePersistentUserCallback =
      chromeos::DBusMethodCallback<::user_data_auth::CreatePersistentUserReply>;
  using PreparePersistentVaultCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::PreparePersistentVaultReply>;
  using PrepareVaultForMigrationCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::PrepareVaultForMigrationReply>;

  using StartMigrateToDircryptoCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::StartMigrateToDircryptoReply>;
  using NeedsDircryptoMigrationCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::NeedsDircryptoMigrationReply>;

  using GetArcDiskFeaturesCallback =
      chromeos::DBusMethodCallback<::user_data_auth::GetArcDiskFeaturesReply>;

  using GetRecoverableKeyStoresCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::GetRecoverableKeyStoresReply>;

  using SetUserDataStorageWriteEnabledCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::SetUserDataStorageWriteEnabledReply>;

  // Not copyable or movable.
  UserDataAuthClient(const UserDataAuthClient&) = delete;
  UserDataAuthClient& operator=(const UserDataAuthClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Override the global instance for testing. Must only be called in unit
  // tests, which bypass the normal browser startup and shutdown sequence. Use
  // InitializeFake or OverrideGlobalInstance in FakeUserDataAuth for browser
  // tests.
  static void OverrideGlobalInstanceForTesting(UserDataAuthClient*);

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

  // Adds a fingerprint auth observer.
  virtual void AddFingerprintAuthObserver(
      FingerprintAuthObserver* observer) = 0;

  // Removes a fingerprint auth observer if added.
  virtual void RemoveFingerprintAuthObserver(
      FingerprintAuthObserver* observer) = 0;

  // Adds a PrepareAuthFactorProgress observer.
  virtual void AddPrepareAuthFactorProgressObserver(
      PrepareAuthFactorProgressObserver* observer) = 0;

  // Removes a PrepareAuthFactorProgress observer if added.
  virtual void RemovePrepareAuthFactorProgressObserver(
      PrepareAuthFactorProgressObserver* observer) = 0;

  virtual void AddAuthFactorStatusUpdateObserver(
      AuthFactorStatusUpdateObserver* observer) = 0;

  virtual void RemoveAuthFactorStatusUpdateObserver(
      AuthFactorStatusUpdateObserver* observer) = 0;

  // Actual DBus Methods:

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) = 0;

  // Queries if user's vault is mounted.
  virtual void IsMounted(const ::user_data_auth::IsMountedRequest& request,
                         IsMountedCallback callback) = 0;

  // Queries user's vault properties.
  virtual void GetVaultProperties(
      const ::user_data_auth::GetVaultPropertiesRequest& request,
      GetVaultPropertiesCallback callback) = 0;

  // Unmounts user's vault.
  virtual void Unmount(const ::user_data_auth::UnmountRequest& request,
                       UnmountCallback callback) = 0;

  // Removes user's vault.
  virtual void Remove(const ::user_data_auth::RemoveRequest& request,
                      RemoveCallback callback) = 0;

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

  // Makes user directory available for migration.
  virtual void PrepareVaultForMigration(
      const ::user_data_auth::PrepareVaultForMigrationRequest& request,
      PrepareVaultForMigrationCallback callback) = 0;

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

  // This call will be used in the case of a user wanting
  // to update an AuthFactor's metadata. (E.g. Changing the user specified
  // name).
  virtual void UpdateAuthFactorMetadata(
      const ::user_data_auth::UpdateAuthFactorMetadataRequest& request,
      UpdateAuthFactorMetadataCallback callback) = 0;

  // This call will be used in the case of a user wanting to remove an existing
  // Authfactor and add a new one to replace it. (E.g. Changing to local
  // password from Gaia password).
  virtual void ReplaceAuthFactor(
      const ::user_data_auth::ReplaceAuthFactorRequest& request,
      ReplaceAuthFactorCallback callback) = 0;

  // This is called when a user wants to remove an
  // AuthFactor.
  virtual void RemoveAuthFactor(
      const ::user_data_auth::RemoveAuthFactorRequest& request,
      RemoveAuthFactorCallback callback) = 0;

  // This is called to determine all configured AuthFactors as well as supported
  // AuthFactors whenever AuthFactors-based API is used.
  virtual void ListAuthFactors(
      const ::user_data_auth::ListAuthFactorsRequest& request,
      ListAuthFactorsCallback callback) = 0;

  // This is called to get AuthFactor for given label along with optional
  // extended info.
  virtual void GetAuthFactorExtendedInfo(
      const ::user_data_auth::GetAuthFactorExtendedInfoRequest& request,
      GetAuthFactorExtendedInfoCallback callback) = 0;

  // This is called when a user wants to get an AuthSession status.
  virtual void GetAuthSessionStatus(
      const ::user_data_auth::GetAuthSessionStatusRequest& request,
      GetAuthSessionStatusCallback callback) = 0;

  // This is called to enable asynchronous auth factors (like Fingerprint).
  // Note that caller needs to add PrepareAuthFactorProgressObserver before this
  // call.
  virtual void PrepareAuthFactor(
      const ::user_data_auth::PrepareAuthFactorRequest& request,
      PrepareAuthFactorCallback callback) = 0;

  // Counterpart for `PrepareAuthFactor`, method is called to disable particular
  // asynchronous auth factor (like Fingerprint).
  // Note that caller needs to remove PrepareAuthFactorProgressObserver after
  // this call.
  virtual void TerminateAuthFactor(
      const ::user_data_auth::TerminateAuthFactorRequest& request,
      TerminateAuthFactorCallback callback) = 0;

  // Retrieve the ARC-related disk features supported.
  virtual void GetArcDiskFeatures(
      const ::user_data_auth::GetArcDiskFeaturesRequest& request,
      GetArcDiskFeaturesCallback callback) = 0;

  // Retrieve LSKF-wrapped key material for upload to a remote recovery service.
  virtual void GetRecoverableKeyStores(
      const ::user_data_auth::GetRecoverableKeyStoresRequest& request,
      GetRecoverableKeyStoresCallback callback) = 0;

  // Enable/disable write access permissions to MyFiles directory.
  virtual void SetUserDataStorageWriteEnabled(
      const ::user_data_auth::SetUserDataStorageWriteEnabledRequest& request,
      SetUserDataStorageWriteEnabledCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  UserDataAuthClient();
  virtual ~UserDataAuthClient();
};

}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<
    ash::UserDataAuthClient,
    ash::UserDataAuthClient::FingerprintAuthObserver> {
  static void AddObserver(
      ash::UserDataAuthClient* source,
      ash::UserDataAuthClient::FingerprintAuthObserver* observer) {
    source->AddFingerprintAuthObserver(observer);
  }
  static void RemoveObserver(
      ash::UserDataAuthClient* source,
      ash::UserDataAuthClient::FingerprintAuthObserver* observer) {
    source->RemoveFingerprintAuthObserver(observer);
  }
};

}  // namespace base

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_
