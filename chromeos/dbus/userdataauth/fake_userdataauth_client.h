// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_

#include "chromeos/dbus/userdataauth/userdataauth_client.h"

#include "base/component_export.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"

namespace chromeos {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) FakeUserDataAuthClient
    : public UserDataAuthClient {
 public:
  FakeUserDataAuthClient();
  ~FakeUserDataAuthClient() override;

  // Not copyable or movable.
  FakeUserDataAuthClient(const FakeUserDataAuthClient&) = delete;
  FakeUserDataAuthClient& operator=(const FakeUserDataAuthClient&) = delete;

  // UserDataAuthClient override:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void IsMounted(const ::user_data_auth::IsMountedRequest& request,
                 IsMountedCallback callback) override;
  void Unmount(const ::user_data_auth::UnmountRequest& request,
               UnmountCallback callback) override;
  void Mount(const ::user_data_auth::MountRequest& request,
             MountCallback callback) override;
  void Remove(const ::user_data_auth::RemoveRequest& request,
              RemoveCallback callback) override;
  void Rename(const ::user_data_auth::RenameRequest& request,
              RenameCallback callback) override;
  void GetKeyData(const ::user_data_auth::GetKeyDataRequest& request,
                  GetKeyDataCallback callback) override;
  void CheckKey(const ::user_data_auth::CheckKeyRequest& request,
                CheckKeyCallback callback) override;
  void AddKey(const ::user_data_auth::AddKeyRequest& request,
              AddKeyCallback callback) override;
  void RemoveKey(const ::user_data_auth::RemoveKeyRequest& request,
                 RemoveKeyCallback callback) override;
  void MassRemoveKeys(const ::user_data_auth::MassRemoveKeysRequest& request,
                      MassRemoveKeysCallback callback) override;
  void MigrateKey(const ::user_data_auth::MigrateKeyRequest& request,
                  MigrateKeyCallback callback) override;
  void StartFingerprintAuthSession(
      const ::user_data_auth::StartFingerprintAuthSessionRequest& request,
      StartFingerprintAuthSessionCallback callback) override;
  void EndFingerprintAuthSession(
      const ::user_data_auth::EndFingerprintAuthSessionRequest& request,
      EndFingerprintAuthSessionCallback callback) override;
  void StartMigrateToDircrypto(
      const ::user_data_auth::StartMigrateToDircryptoRequest& request,
      StartMigrateToDircryptoCallback callback) override;
  void NeedsDircryptoMigration(
      const ::user_data_auth::NeedsDircryptoMigrationRequest& request,
      NeedsDircryptoMigrationCallback callback) override;
  void GetSupportedKeyPolicies(
      const ::user_data_auth::GetSupportedKeyPoliciesRequest& request,
      GetSupportedKeyPoliciesCallback callback) override;
  void GetAccountDiskUsage(
      const ::user_data_auth::GetAccountDiskUsageRequest& request,
      GetAccountDiskUsageCallback callback) override;
  void StartAuthSession(
      const ::user_data_auth::StartAuthSessionRequest& request,
      StartAuthSessionCallback callback) override;
  void AuthenticateAuthSession(
      const ::user_data_auth::AuthenticateAuthSessionRequest& request,
      AuthenticateAuthSessionCallback callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_
