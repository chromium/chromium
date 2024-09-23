// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_CRYPTOHOME_PKCS11_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_CRYPTOHOME_PKCS11_CLIENT_H_

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

// CryptohomePkcs11Client is used to communicate with the
// org.chromium.CryptohomePkcs11 interface within org.chromium.UserDataAuth
// service exposed by cryptohomed. All method should be called from the origin
// thread (UI thread) which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) CryptohomePkcs11Client {
 public:
  using Pkcs11IsTpmTokenReadyCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::Pkcs11IsTpmTokenReadyReply>;
  using Pkcs11GetTpmTokenInfoCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::Pkcs11GetTpmTokenInfoReply>;

  // Not copyable or movable.
  CryptohomePkcs11Client(const CryptohomePkcs11Client&) = delete;
  CryptohomePkcs11Client& operator=(const CryptohomePkcs11Client&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static CryptohomePkcs11Client* Get();

  // Actual DBus Methods:

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) = 0;

  // Checks if user's PKCS#11 token (chaps) is ready.
  virtual void Pkcs11IsTpmTokenReady(
      const ::user_data_auth::Pkcs11IsTpmTokenReadyRequest& request,
      Pkcs11IsTpmTokenReadyCallback callback) = 0;

  // Retrieves the information required to use user's PKCS#11 token.
  virtual void Pkcs11GetTpmTokenInfo(
      const ::user_data_auth::Pkcs11GetTpmTokenInfoRequest& request,
      Pkcs11GetTpmTokenInfoCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  CryptohomePkcs11Client();
  virtual ~CryptohomePkcs11Client();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_CRYPTOHOME_PKCS11_CLIENT_H_
