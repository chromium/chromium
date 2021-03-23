// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/userdataauth/fake_cryptohome_pkcs11_client.h"

#include "base/notreached.h"

namespace chromeos {

FakeCryptohomePkcs11Client::FakeCryptohomePkcs11Client() = default;

FakeCryptohomePkcs11Client::~FakeCryptohomePkcs11Client() = default;

void FakeCryptohomePkcs11Client::Pkcs11IsTpmTokenReady(
    const ::user_data_auth::Pkcs11IsTpmTokenReadyRequest& request,
    Pkcs11IsTpmTokenReadyCallback callback) {
  NOTIMPLEMENTED();
}
void FakeCryptohomePkcs11Client::Pkcs11GetTpmTokenInfo(
    const ::user_data_auth::Pkcs11GetTpmTokenInfoRequest& request,
    Pkcs11GetTpmTokenInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeCryptohomePkcs11Client::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
