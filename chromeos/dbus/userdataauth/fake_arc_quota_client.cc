// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/userdataauth/fake_arc_quota_client.h"

#include "base/notreached.h"

namespace chromeos {

FakeArcQuotaClient::FakeArcQuotaClient() = default;

FakeArcQuotaClient::~FakeArcQuotaClient() = default;

void FakeArcQuotaClient::GetArcDiskFeatures(
    const ::user_data_auth::GetArcDiskFeaturesRequest& request,
    GetArcDiskFeaturesCallback callback) {
  NOTIMPLEMENTED();
}
void FakeArcQuotaClient::GetCurrentSpaceForArcUid(
    const ::user_data_auth::GetCurrentSpaceForArcUidRequest& request,
    GetCurrentSpaceForArcUidCallback callback) {
  NOTIMPLEMENTED();
}
void FakeArcQuotaClient::GetCurrentSpaceForArcGid(
    const ::user_data_auth::GetCurrentSpaceForArcGidRequest& request,
    GetCurrentSpaceForArcGidCallback callback) {
  NOTIMPLEMENTED();
}
void FakeArcQuotaClient::GetCurrentSpaceForArcProjectId(
    const ::user_data_auth::GetCurrentSpaceForArcProjectIdRequest& request,
    GetCurrentSpaceForArcProjectIdCallback callback) {
  NOTIMPLEMENTED();
}
void FakeArcQuotaClient::SetProjectId(
    const ::user_data_auth::SetProjectIdRequest& request,
    SetProjectIdCallback callback) {
  NOTIMPLEMENTED();
}

void FakeArcQuotaClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
