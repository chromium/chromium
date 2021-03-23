// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/userdataauth/fake_cryptohome_misc_client.h"

#include "base/notreached.h"

namespace chromeos {

FakeCryptohomeMiscClient::FakeCryptohomeMiscClient() = default;

FakeCryptohomeMiscClient::~FakeCryptohomeMiscClient() = default;

void FakeCryptohomeMiscClient::GetSystemSalt(
    const ::user_data_auth::GetSystemSaltRequest& request,
    GetSystemSaltCallback callback) {
  NOTIMPLEMENTED();
}
void FakeCryptohomeMiscClient::GetSanitizedUsername(
    const ::user_data_auth::GetSanitizedUsernameRequest& request,
    GetSanitizedUsernameCallback callback) {
  NOTIMPLEMENTED();
}
void FakeCryptohomeMiscClient::GetLoginStatus(
    const ::user_data_auth::GetLoginStatusRequest& request,
    GetLoginStatusCallback callback) {
  NOTIMPLEMENTED();
}
void FakeCryptohomeMiscClient::LockToSingleUserMountUntilReboot(
    const ::user_data_auth::LockToSingleUserMountUntilRebootRequest& request,
    LockToSingleUserMountUntilRebootCallback callback) {
  NOTIMPLEMENTED();
}
void FakeCryptohomeMiscClient::GetRsuDeviceId(
    const ::user_data_auth::GetRsuDeviceIdRequest& request,
    GetRsuDeviceIdCallback callback) {
  NOTIMPLEMENTED();
}
void FakeCryptohomeMiscClient::CheckHealth(
    const ::user_data_auth::CheckHealthRequest& request,
    CheckHealthCallback callback) {
  NOTIMPLEMENTED();
}

base::Optional<::user_data_auth::GetSanitizedUsernameReply>
FakeCryptohomeMiscClient::BlockingGetSanitizedUsername(
    const ::user_data_auth::GetSanitizedUsernameRequest& request) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

void FakeCryptohomeMiscClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
