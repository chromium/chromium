// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_FAKE_CRYPTOHOME_PKCS11_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_FAKE_CRYPTOHOME_PKCS11_CLIENT_H_

#include "chromeos/dbus/userdataauth/cryptohome_pkcs11_client.h"

#include "base/component_export.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"

namespace chromeos {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) FakeCryptohomePkcs11Client
    : public CryptohomePkcs11Client {
 public:
  FakeCryptohomePkcs11Client();
  ~FakeCryptohomePkcs11Client() override;

  // Not copyable or movable.
  FakeCryptohomePkcs11Client(const FakeCryptohomePkcs11Client&) = delete;
  FakeCryptohomePkcs11Client& operator=(const FakeCryptohomePkcs11Client&) =
      delete;

  // CryptohomePkcs11Client override:
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void Pkcs11IsTpmTokenReady(
      const ::user_data_auth::Pkcs11IsTpmTokenReadyRequest& request,
      Pkcs11IsTpmTokenReadyCallback callback) override;
  void Pkcs11GetTpmTokenInfo(
      const ::user_data_auth::Pkcs11GetTpmTokenInfoRequest& request,
      Pkcs11GetTpmTokenInfoCallback callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_USERDATAAUTH_FAKE_CRYPTOHOME_PKCS11_CLIENT_H_
