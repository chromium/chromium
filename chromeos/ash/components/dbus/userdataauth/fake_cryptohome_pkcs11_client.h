// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_CRYPTOHOME_PKCS11_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_CRYPTOHOME_PKCS11_CLIENT_H_

#include "chromeos/ash/components/dbus/userdataauth/cryptohome_pkcs11_client.h"

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"

namespace ash {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) FakeCryptohomePkcs11Client
    : public CryptohomePkcs11Client {
 public:
  FakeCryptohomePkcs11Client();
  ~FakeCryptohomePkcs11Client() override;

  // Not copyable or movable.
  FakeCryptohomePkcs11Client(const FakeCryptohomePkcs11Client&) = delete;
  FakeCryptohomePkcs11Client& operator=(const FakeCryptohomePkcs11Client&) =
      delete;

  // Checks that a FakeCryptohomePkcs11Client instance was initialized and
  // returns it.
  static FakeCryptohomePkcs11Client* Get();

  // CryptohomePkcs11Client override:
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void Pkcs11IsTpmTokenReady(
      const ::user_data_auth::Pkcs11IsTpmTokenReadyRequest& request,
      Pkcs11IsTpmTokenReadyCallback callback) override;
  void Pkcs11GetTpmTokenInfo(
      const ::user_data_auth::Pkcs11GetTpmTokenInfoRequest& request,
      Pkcs11GetTpmTokenInfoCallback callback) override;

  // WaitForServiceToBeAvailable() related:

  // Changes the behavior of WaitForServiceToBeAvailable(). This method runs
  // pending callbacks if is_available is true.
  void SetServiceIsAvailable(bool is_available);

  // Runs pending availability callbacks reporting that the service is
  // unavailable. Expects service not to be available when called.
  void ReportServiceIsNotAvailable();

 private:
  // WaitForServiceToBeAvailable() related fields:

  // If set, we tell callers that service is available.
  bool service_is_available_ = true;

  // If set, WaitForServiceToBeAvailable will run the callback, even if service
  // is not available (instead of adding the callback to pending callback list).
  bool service_reported_not_available_ = false;

  // The list of callbacks passed to WaitForServiceToBeAvailable when the
  // service wasn't available.
  std::vector<chromeos::WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_CRYPTOHOME_PKCS11_CLIENT_H_
