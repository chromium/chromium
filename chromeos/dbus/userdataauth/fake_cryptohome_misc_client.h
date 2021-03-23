// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_FAKE_CRYPTOHOME_MISC_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_FAKE_CRYPTOHOME_MISC_CLIENT_H_

#include "chromeos/dbus/userdataauth/cryptohome_misc_client.h"

#include "base/component_export.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"

namespace chromeos {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) FakeCryptohomeMiscClient
    : public CryptohomeMiscClient {
 public:
  FakeCryptohomeMiscClient();
  ~FakeCryptohomeMiscClient() override;

  // Not copyable or movable.
  FakeCryptohomeMiscClient(const FakeCryptohomeMiscClient&) = delete;
  FakeCryptohomeMiscClient& operator=(const FakeCryptohomeMiscClient&) = delete;

  // CryptohomeMiscClient override:
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void GetSystemSalt(const ::user_data_auth::GetSystemSaltRequest& request,
                     GetSystemSaltCallback callback) override;
  void GetSanitizedUsername(
      const ::user_data_auth::GetSanitizedUsernameRequest& request,
      GetSanitizedUsernameCallback callback) override;
  void GetLoginStatus(const ::user_data_auth::GetLoginStatusRequest& request,
                      GetLoginStatusCallback callback) override;
  void LockToSingleUserMountUntilReboot(
      const ::user_data_auth::LockToSingleUserMountUntilRebootRequest& request,
      LockToSingleUserMountUntilRebootCallback callback) override;
  void GetRsuDeviceId(const ::user_data_auth::GetRsuDeviceIdRequest& request,
                      GetRsuDeviceIdCallback callback) override;
  void CheckHealth(const ::user_data_auth::CheckHealthRequest& request,
                   CheckHealthCallback callback) override;

  base::Optional<::user_data_auth::GetSanitizedUsernameReply>
  BlockingGetSanitizedUsername(
      const ::user_data_auth::GetSanitizedUsernameRequest& request) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_USERDATAAUTH_FAKE_CRYPTOHOME_MISC_CLIENT_H_
