// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/userdataauth/fake_install_attributes_client.h"

#include "base/notreached.h"

namespace chromeos {

FakeInstallAttributesClient::FakeInstallAttributesClient() = default;

FakeInstallAttributesClient::~FakeInstallAttributesClient() = default;

void FakeInstallAttributesClient::InstallAttributesGet(
    const ::user_data_auth::InstallAttributesGetRequest& request,
    InstallAttributesGetCallback callback) {
  NOTIMPLEMENTED();
}
void FakeInstallAttributesClient::InstallAttributesSet(
    const ::user_data_auth::InstallAttributesSetRequest& request,
    InstallAttributesSetCallback callback) {
  NOTIMPLEMENTED();
}
void FakeInstallAttributesClient::InstallAttributesFinalize(
    const ::user_data_auth::InstallAttributesFinalizeRequest& request,
    InstallAttributesFinalizeCallback callback) {
  NOTIMPLEMENTED();
}
void FakeInstallAttributesClient::InstallAttributesGetStatus(
    const ::user_data_auth::InstallAttributesGetStatusRequest& request,
    InstallAttributesGetStatusCallback callback) {
  NOTIMPLEMENTED();
}
void FakeInstallAttributesClient::RemoveFirmwareManagementParameters(
    const ::user_data_auth::RemoveFirmwareManagementParametersRequest& request,
    RemoveFirmwareManagementParametersCallback callback) {
  NOTIMPLEMENTED();
}
void FakeInstallAttributesClient::SetFirmwareManagementParameters(
    const ::user_data_auth::SetFirmwareManagementParametersRequest& request,
    SetFirmwareManagementParametersCallback callback) {
  NOTIMPLEMENTED();
}
base::Optional<::user_data_auth::InstallAttributesGetReply>
FakeInstallAttributesClient::BlockingInstallAttributesGet(
    const ::user_data_auth::InstallAttributesGetRequest& request) {
  NOTIMPLEMENTED();
  return base::nullopt;
}
base::Optional<::user_data_auth::InstallAttributesSetReply>
FakeInstallAttributesClient::BlockingInstallAttributesSet(
    const ::user_data_auth::InstallAttributesSetRequest& request) {
  NOTIMPLEMENTED();
  return base::nullopt;
}
base::Optional<::user_data_auth::InstallAttributesFinalizeReply>
FakeInstallAttributesClient::BlockingInstallAttributesFinalize(
    const ::user_data_auth::InstallAttributesFinalizeRequest& request) {
  NOTIMPLEMENTED();
  return base::nullopt;
}
base::Optional<::user_data_auth::InstallAttributesGetStatusReply>
FakeInstallAttributesClient::BlockingInstallAttributesGetStatus(
    const ::user_data_auth::InstallAttributesGetStatusRequest& request) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

void FakeInstallAttributesClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
