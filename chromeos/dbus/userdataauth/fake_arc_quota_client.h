// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_FAKE_ARC_QUOTA_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_FAKE_ARC_QUOTA_CLIENT_H_

#include "chromeos/dbus/userdataauth/arc_quota_client.h"

#include "base/component_export.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"

namespace chromeos {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) FakeArcQuotaClient
    : public ArcQuotaClient {
 public:
  FakeArcQuotaClient();
  ~FakeArcQuotaClient() override;

  // Not copyable or movable.
  FakeArcQuotaClient(const FakeArcQuotaClient&) = delete;
  FakeArcQuotaClient& operator=(const FakeArcQuotaClient&) = delete;

  // ArcQuotaClient override:
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void GetArcDiskFeatures(
      const ::user_data_auth::GetArcDiskFeaturesRequest& request,
      GetArcDiskFeaturesCallback callback) override;
  void GetCurrentSpaceForArcUid(
      const ::user_data_auth::GetCurrentSpaceForArcUidRequest& request,
      GetCurrentSpaceForArcUidCallback callback) override;
  void GetCurrentSpaceForArcGid(
      const ::user_data_auth::GetCurrentSpaceForArcGidRequest& request,
      GetCurrentSpaceForArcGidCallback callback) override;
  void GetCurrentSpaceForArcProjectId(
      const ::user_data_auth::GetCurrentSpaceForArcProjectIdRequest& request,
      GetCurrentSpaceForArcProjectIdCallback callback) override;
  void SetProjectId(const ::user_data_auth::SetProjectIdRequest& request,
                    SetProjectIdCallback callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_USERDATAAUTH_FAKE_ARC_QUOTA_CLIENT_H_
