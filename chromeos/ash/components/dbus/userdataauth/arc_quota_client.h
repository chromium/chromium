// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_ARC_QUOTA_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_ARC_QUOTA_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"

namespace dbus {
class Bus;
}

namespace ash {

// ArcQuotaClient is used to communicate with the org.chromium.ArcQuota
// interface within org.chromium.UserDataAuth service exposed by cryptohomed.
// All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) ArcQuotaClient {
 public:
  using GetArcDiskFeaturesCallback =
      chromeos::DBusMethodCallback<::user_data_auth::GetArcDiskFeaturesReply>;
  using GetCurrentSpaceForArcUidCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::GetCurrentSpaceForArcUidReply>;
  using GetCurrentSpaceForArcGidCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::GetCurrentSpaceForArcGidReply>;
  using GetCurrentSpaceForArcProjectIdCallback = chromeos::DBusMethodCallback<
      ::user_data_auth::GetCurrentSpaceForArcProjectIdReply>;

  // Not copyable or movable.
  ArcQuotaClient(const ArcQuotaClient&) = delete;
  ArcQuotaClient& operator=(const ArcQuotaClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static ArcQuotaClient* Get();

  // Actual DBus Methods:

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) = 0;

  // Retrieve the ARC-related disk features supported.
  virtual void GetArcDiskFeatures(
      const ::user_data_auth::GetArcDiskFeaturesRequest& request,
      GetArcDiskFeaturesCallback callback) = 0;

  // Retrieve the disk space usage for ARC's user.
  virtual void GetCurrentSpaceForArcUid(
      const ::user_data_auth::GetCurrentSpaceForArcUidRequest& request,
      GetCurrentSpaceForArcUidCallback callback) = 0;

  // Retrieve the disk space usage for ARC's group.
  virtual void GetCurrentSpaceForArcGid(
      const ::user_data_auth::GetCurrentSpaceForArcGidRequest& request,
      GetCurrentSpaceForArcGidCallback callback) = 0;

  // Retrieve the disk space usage for an ARC project ID.
  virtual void GetCurrentSpaceForArcProjectId(
      const ::user_data_auth::GetCurrentSpaceForArcProjectIdRequest& request,
      GetCurrentSpaceForArcProjectIdCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  ArcQuotaClient();
  virtual ~ArcQuotaClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_ARC_QUOTA_CLIENT_H_
