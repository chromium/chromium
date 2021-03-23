// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_FAKE_INSTALL_ATTRIBUTES_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_FAKE_INSTALL_ATTRIBUTES_CLIENT_H_

#include "chromeos/dbus/userdataauth/install_attributes_client.h"

#include "base/component_export.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"

namespace chromeos {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) FakeInstallAttributesClient
    : public InstallAttributesClient {
 public:
  FakeInstallAttributesClient();
  ~FakeInstallAttributesClient() override;

  // Not copyable or movable.
  FakeInstallAttributesClient(const FakeInstallAttributesClient&) = delete;
  FakeInstallAttributesClient& operator=(const FakeInstallAttributesClient&) =
      delete;

  // InstallAttributesClient override:
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void InstallAttributesGet(
      const ::user_data_auth::InstallAttributesGetRequest& request,
      InstallAttributesGetCallback callback) override;
  void InstallAttributesSet(
      const ::user_data_auth::InstallAttributesSetRequest& request,
      InstallAttributesSetCallback callback) override;
  void InstallAttributesFinalize(
      const ::user_data_auth::InstallAttributesFinalizeRequest& request,
      InstallAttributesFinalizeCallback callback) override;
  void InstallAttributesGetStatus(
      const ::user_data_auth::InstallAttributesGetStatusRequest& request,
      InstallAttributesGetStatusCallback callback) override;
  void RemoveFirmwareManagementParameters(
      const ::user_data_auth::RemoveFirmwareManagementParametersRequest&
          request,
      RemoveFirmwareManagementParametersCallback callback) override;
  void SetFirmwareManagementParameters(
      const ::user_data_auth::SetFirmwareManagementParametersRequest& request,
      SetFirmwareManagementParametersCallback callback) override;
  base::Optional<::user_data_auth::InstallAttributesGetReply>
  BlockingInstallAttributesGet(
      const ::user_data_auth::InstallAttributesGetRequest& request) override;
  base::Optional<::user_data_auth::InstallAttributesSetReply>
  BlockingInstallAttributesSet(
      const ::user_data_auth::InstallAttributesSetRequest& request) override;
  base::Optional<::user_data_auth::InstallAttributesFinalizeReply>
  BlockingInstallAttributesFinalize(
      const ::user_data_auth::InstallAttributesFinalizeRequest& request)
      override;
  base::Optional<::user_data_auth::InstallAttributesGetStatusReply>
  BlockingInstallAttributesGetStatus(
      const ::user_data_auth::InstallAttributesGetStatusRequest& request)
      override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_USERDATAAUTH_FAKE_INSTALL_ATTRIBUTES_CLIENT_H_
