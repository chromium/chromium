// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_MOCK_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_MOCK_USERDATAAUTH_CLIENT_H_

#include "chromeos/dbus/userdataauth/userdataauth_client.h"

#include "base/component_export.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) MockUserDataAuthClient
    : public UserDataAuthClient {
 public:
  MockUserDataAuthClient();
  ~MockUserDataAuthClient() override;

  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  MOCK_METHOD(void,
              IsMounted,
              (const ::user_data_auth::IsMountedRequest& request,
               IsMountedCallback callback),
              (override));
  MOCK_METHOD(void,
              Unmount,
              (const ::user_data_auth::UnmountRequest& request,
               UnmountCallback callback),
              (override));
  MOCK_METHOD(void,
              Mount,
              (const ::user_data_auth::MountRequest& request,
               MountCallback callback),
              (override));
  MOCK_METHOD(void,
              Remove,
              (const ::user_data_auth::RemoveRequest& request,
               RemoveCallback callback),
              (override));
  MOCK_METHOD(void,
              GetKeyData,
              (const ::user_data_auth::GetKeyDataRequest& request,
               GetKeyDataCallback callback),
              (override));
  MOCK_METHOD(void,
              CheckKey,
              (const ::user_data_auth::CheckKeyRequest& request,
               CheckKeyCallback callback),
              (override));
  MOCK_METHOD(void,
              AddKey,
              (const ::user_data_auth::AddKeyRequest& request,
               AddKeyCallback callback),
              (override));
  MOCK_METHOD(void,
              RemoveKey,
              (const ::user_data_auth::RemoveKeyRequest& request,
               RemoveKeyCallback callback),
              (override));
  MOCK_METHOD(void,
              MassRemoveKeys,
              (const ::user_data_auth::MassRemoveKeysRequest& request,
               MassRemoveKeysCallback callback),
              (override));
  MOCK_METHOD(void,
              MigrateKey,
              (const ::user_data_auth::MigrateKeyRequest& request,
               MigrateKeyCallback callback),
              (override));
  MOCK_METHOD(
      void,
      StartFingerprintAuthSession,
      (const ::user_data_auth::StartFingerprintAuthSessionRequest& request,
       StartFingerprintAuthSessionCallback callback),
      (override));
  MOCK_METHOD(
      void,
      EndFingerprintAuthSession,
      (const ::user_data_auth::EndFingerprintAuthSessionRequest& request,
       EndFingerprintAuthSessionCallback callback),
      (override));
  MOCK_METHOD(void,
              StartMigrateToDircrypto,
              (const ::user_data_auth::StartMigrateToDircryptoRequest& request,
               StartMigrateToDircryptoCallback callback),
              (override));
  MOCK_METHOD(void,
              NeedsDircryptoMigration,
              (const ::user_data_auth::NeedsDircryptoMigrationRequest& request,
               NeedsDircryptoMigrationCallback callback),
              (override));
  MOCK_METHOD(void,
              GetSupportedKeyPolicies,
              (const ::user_data_auth::GetSupportedKeyPoliciesRequest& request,
               GetSupportedKeyPoliciesCallback callback),
              (override));
  MOCK_METHOD(void,
              GetAccountDiskUsage,
              (const ::user_data_auth::GetAccountDiskUsageRequest& request,
               GetAccountDiskUsageCallback callback),
              (override));
  MOCK_METHOD(void,
              StartAuthSession,
              (const ::user_data_auth::StartAuthSessionRequest& request,
               StartAuthSessionCallback callback),
              (override));
  MOCK_METHOD(void,
              AuthenticateAuthSession,
              (const ::user_data_auth::AuthenticateAuthSessionRequest& request,
               AuthenticateAuthSessionCallback callback),
              (override));
  MOCK_METHOD(void,
              AddCredentials,
              (const ::user_data_auth::AddCredentialsRequest& request,
               AddCredentialsCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateCredential,
              (const ::user_data_auth::UpdateCredentialRequest& request,
               UpdateCredentialCallback callback),
              (override));
  MOCK_METHOD(void,
              PrepareGuestVault,
              (const ::user_data_auth::PrepareGuestVaultRequest& request,
               PrepareGuestVaultCallback callback),
              (override));
  MOCK_METHOD(void,
              PrepareEphemeralVault,
              (const ::user_data_auth::PrepareEphemeralVaultRequest& request,
               PrepareEphemeralVaultCallback callback),
              (override));
  MOCK_METHOD(void,
              CreatePersistentUser,
              (const ::user_data_auth::CreatePersistentUserRequest& request,
               CreatePersistentUserCallback callback),
              (override));
  MOCK_METHOD(void,
              PreparePersistentVault,
              (const ::user_data_auth::PreparePersistentVaultRequest& request,
               PreparePersistentVaultCallback callback),
              (override));
  MOCK_METHOD(void,
              PrepareVaultForMigration,
              (const ::user_data_auth::PrepareVaultForMigrationRequest& request,
               PrepareVaultForMigrationCallback callback),
              (override));
  MOCK_METHOD(void,
              InvalidateAuthSession,
              (const ::user_data_auth::InvalidateAuthSessionRequest& request,
               InvalidateAuthSessionCallback callback),
              (override));
  MOCK_METHOD(void,
              ExtendAuthSession,
              (const ::user_data_auth::ExtendAuthSessionRequest& request,
               ExtendAuthSessionCallback callback),
              (override));
  MOCK_METHOD(void,
              AddAuthFactor,
              (const ::user_data_auth::AddAuthFactorRequest& request,
               AddAuthFactorCallback callback),
              (override));
  MOCK_METHOD(void,
              AuthenticateAuthFactor,
              (const ::user_data_auth::AuthenticateAuthFactorRequest& request,
               AuthenticateAuthFactorCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateAuthFactor,
              (const ::user_data_auth::UpdateAuthFactorRequest& request,
               UpdateAuthFactorCallback callback),
              (override));
  MOCK_METHOD(void,
              RemoveAuthFactor,
              (const ::user_data_auth::RemoveAuthFactorRequest& request,
               RemoveAuthFactorCallback callback),
              (override));
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::UserDataAuthClient;
}

#endif  // CHROMEOS_DBUS_USERDATAAUTH_MOCK_USERDATAAUTH_CLIENT_H_
