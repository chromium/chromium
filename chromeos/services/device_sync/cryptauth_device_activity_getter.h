// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_ACTIVITY_GETTER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_ACTIVITY_GETTER_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"

namespace chromeos {

namespace device_sync {

// Handles the GetDevicesActivityStatus call of the CryptAuth v2 DeviceSync
// protocol. Returns the device activity statuses for the user's devices.
//
// A CryptAuthDeviceActivityGetter object is designed to be used for only one
// GetDevicesActivityStatus() call. For a new attempt, a new object should be
// created.
class CryptAuthDeviceActivityGetter {
 public:
  using DeviceActivityStatusResult =
      std::vector<mojom::DeviceActivityStatusPtr>;
  using GetDeviceActivityStatusAttemptFinishedCallback =
      base::OnceCallback<void(DeviceActivityStatusResult)>;
  using GetDeviceActivityStatusAttemptErrorCallback =
      base::OnceCallback<void(NetworkRequestError)>;

  virtual ~CryptAuthDeviceActivityGetter();

  // Starts the GetDevicesActivityStatus portion of the CryptAuth v2 DeviceSync
  // flow, retrieving the user's device activity status.
  void GetDevicesActivityStatus(
      GetDeviceActivityStatusAttemptFinishedCallback success_callback,
      GetDeviceActivityStatusAttemptErrorCallback error_callback);

 protected:
  CryptAuthDeviceActivityGetter();

  virtual void OnAttemptStarted() = 0;

  void FinishAttemptSuccessfully(
      DeviceActivityStatusResult device_activity_status);
  void FinishAttemptWithError(NetworkRequestError network_request_error);

 private:
  GetDeviceActivityStatusAttemptFinishedCallback success_callback_;
  GetDeviceActivityStatusAttemptErrorCallback error_callback_;
  bool was_get_device_activity_getter_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthDeviceActivityGetter);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_ACTIVITY_GETTER_H_
