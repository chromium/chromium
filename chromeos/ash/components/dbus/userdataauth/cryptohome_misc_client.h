// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_CRYPTOHOME_MISC_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_CRYPTOHOME_MISC_CLIENT_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace dbus {
class Bus;
}

namespace ash {

// CryptohomeMiscClient is used to communicate with the
// org.chromium.CryptohomeMisc interface within org.chromium.UserDataAuth
// service exposed by cryptohomed. All method should be called from the origin
// thread (UI thread) which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) CryptohomeMiscClient {
 public:
  using GetSystemSaltCallback =
      chromeos::DBusMethodCallback<::user_data_auth::GetSystemSaltReply>;
  using GetSanitizedUsernameCallback =
      chromeos::DBusMethodCallback<::user_data_auth::GetSanitizedUsernameReply>;
  using GetLoginStatusCallback =
      chromeos::DBusMethodCallback<::user_data_auth::GetLoginStatusReply>;
  using LockToSingleUserMountUntilRebootCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::LockToSingleUserMountUntilRebootReply>;
  using GetRsuDeviceIdCallback =
      chromeos::DBusMethodCallback<::user_data_auth::GetRsuDeviceIdReply>;

  // Not copyable or movable.
  CryptohomeMiscClient(const CryptohomeMiscClient&) = delete;
  CryptohomeMiscClient& operator=(const CryptohomeMiscClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static CryptohomeMiscClient* Get();

  // Actual DBus Methods:

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) = 0;

  // Retrieves the system salt.
  virtual void GetSystemSalt(
      const ::user_data_auth::GetSystemSaltRequest& request,
      GetSystemSaltCallback callback) = 0;

  // Converts plain username to "hashed" username.
  virtual void GetSanitizedUsername(
      const ::user_data_auth::GetSanitizedUsernameRequest& request,
      GetSanitizedUsernameCallback callback) = 0;

  // Retrieves the login status.
  virtual void GetLoginStatus(
      const ::user_data_auth::GetLoginStatusRequest& request,
      GetLoginStatusCallback callback) = 0;

  // Locks the current boot into single user.
  virtual void LockToSingleUserMountUntilReboot(
      const ::user_data_auth::LockToSingleUserMountUntilRebootRequest& request,
      LockToSingleUserMountUntilRebootCallback callback) = 0;

  // Retrieves the RSU device ID.
  virtual void GetRsuDeviceId(
      const ::user_data_auth::GetRsuDeviceIdRequest& request,
      GetRsuDeviceIdCallback callback) = 0;

  // Blocking version of GetSanitizedUsername().
  virtual std::optional<::user_data_auth::GetSanitizedUsernameReply>
  BlockingGetSanitizedUsername(
      const ::user_data_auth::GetSanitizedUsernameRequest& request) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  CryptohomeMiscClient();
  virtual ~CryptohomeMiscClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_CRYPTOHOME_MISC_CLIENT_H_
