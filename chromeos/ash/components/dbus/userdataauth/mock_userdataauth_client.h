// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_MOCK_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_MOCK_USERDATAAUTH_CLIENT_H_

#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"

#include "base/component_export.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) MockUserDataAuthClient
    : public UserDataAuthClient {
 public:
  MockUserDataAuthClient();
  ~MockUserDataAuthClient() override;

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void AddFingerprintAuthObserver(FingerprintAuthObserver* observer) override;
  void RemoveFingerprintAuthObserver(
      FingerprintAuthObserver* observer) override;
  void AddPrepareAuthFactorProgressObserver(
      PrepareAuthFactorProgressObserver* observer) override;
  void RemovePrepareAuthFactorProgressObserver(
      PrepareAuthFactorProgressObserver* observer) override;
  void AddAuthFactorStatusUpdateObserver(
      AuthFactorStatusUpdateObserver* observer) override;
  void RemoveAuthFactorStatusUpdateObserver(
      AuthFactorStatusUpdateObserver* observer) override;

  MOCK_METHOD(void,
              IsMounted,
              (const ::user_data_auth::IsMountedRequest& request,
               IsMountedCallback callback),
              (override));
  MOCK_METHOD(void,
              GetVaultProperties,
              (const ::user_data_auth::GetVaultPropertiesRequest& request,
               GetVaultPropertiesCallback callback),
              (override));
  MOCK_METHOD(void,
              Unmount,
              (const ::user_data_auth::UnmountRequest& request,
               UnmountCallback callback),
              (override));
  MOCK_METHOD(void,
              Remove,
              (const ::user_data_auth::RemoveRequest& request,
               RemoveCallback callback),
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
              UpdateAuthFactorMetadata,
              (const ::user_data_auth::UpdateAuthFactorMetadataRequest& request,
               UpdateAuthFactorMetadataCallback callback),
              (override));
  MOCK_METHOD(void,
              ReplaceAuthFactor,
              (const ::user_data_auth::ReplaceAuthFactorRequest& request,
               ReplaceAuthFactorCallback callback),
              (override));
  MOCK_METHOD(void,
              ListAuthFactors,
              (const ::user_data_auth::ListAuthFactorsRequest& request,
               ListAuthFactorsCallback callback),
              (override));
  MOCK_METHOD(
      void,
      GetAuthFactorExtendedInfo,
      (const ::user_data_auth::GetAuthFactorExtendedInfoRequest& request,
       GetAuthFactorExtendedInfoCallback callback),
      (override));
  MOCK_METHOD(void,
              RemoveAuthFactor,
              (const ::user_data_auth::RemoveAuthFactorRequest& request,
               RemoveAuthFactorCallback callback),
              (override));
  MOCK_METHOD(void,
              GetAuthSessionStatus,
              (const ::user_data_auth::GetAuthSessionStatusRequest& request,
               GetAuthSessionStatusCallback callback),
              (override));
  MOCK_METHOD(void,
              PrepareAuthFactor,
              (const ::user_data_auth::PrepareAuthFactorRequest& request,
               PrepareAuthFactorCallback callback),
              (override));
  MOCK_METHOD(void,
              TerminateAuthFactor,
              (const ::user_data_auth::TerminateAuthFactorRequest& request,
               TerminateAuthFactorCallback callback),
              (override));
  MOCK_METHOD(void,
              GetArcDiskFeatures,
              (const ::user_data_auth::GetArcDiskFeaturesRequest& request,
               GetArcDiskFeaturesCallback callback),
              (override));
  MOCK_METHOD(void,
              GetRecoverableKeyStores,
              (const ::user_data_auth::GetRecoverableKeyStoresRequest& request,
               GetRecoverableKeyStoresCallback),
              (override));
  MOCK_METHOD(
      void,
      SetUserDataStorageWriteEnabled,
      (const ::user_data_auth::SetUserDataStorageWriteEnabledRequest& request,
       SetUserDataStorageWriteEnabledCallback),
      (override));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_MOCK_USERDATAAUTH_CLIENT_H_
