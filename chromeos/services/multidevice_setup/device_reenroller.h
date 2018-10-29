// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_DEVICE_REENROLLER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_DEVICE_REENROLLER_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace cryptauth {
class GcmDeviceInfoProvider;
}  // namespace cryptauth

namespace chromeos {

namespace multidevice_setup {

// This class re-enrolls the device if the set of supported SoftwareFeatures in
// the GCM device info differs from that of the local device metadata. This
// condition is checked in the constructor and any time the
// DeviceSyncClient::Observer callbacks--OnEnrollmentFinished() and
// OnNewDevicesSync()--are called.
//
// The supported software features listed in GCM device info should be
// considered the new source of truth. Enrollment updates a device's list of
// supported software features (among other things) on the backend to conform
// with the GCM device info. Then, a device sync is necessary to update the
// cache of device information with the latest backend data. The local device
// metadata is part of this cache.
//
// The flow of the class is as follows:
//
//   +-------------------------------------------------------------------------+
//   |                                                                         |
//   |              (From external enrollment)      (From external device sync)|
//   |                         |                                    |          |
//   V                         V                                    V          |
// Start-->Enrollment-->OnEnrollmentFinished-->DeviceSync-->OnNewDevicesSynced-+
//   |                         |
//   |                         |
//   V                         V
//   If features agree, then done
//
// A five-minute retry timer is started at the beginning of the flow. Should any
// step fail, the process will be re-started when the timer fires.
class DeviceReenroller : public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<DeviceReenroller> BuildInstance(
        device_sync::DeviceSyncClient* device_sync_client,
        const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

  ~DeviceReenroller() override;

 private:
  DeviceReenroller(
      device_sync::DeviceSyncClient* device_sync_client,
      const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider,
      std::unique_ptr<base::OneShotTimer> timer);

  void AttemptReenrollmentIfNecessary();
  // Returns a sorted and deduped list of the supported or enabled software
  // features from DeviceSyncClient::GetLocalDeviceMetadata().
  std::vector<cryptauth::SoftwareFeature> GetSupportedFeaturesForLocalDevice();

  // device_sync::DeviceSyncClient::Observer:
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

  device_sync::DeviceSyncClient* device_sync_client_;
  const std::vector<cryptauth::SoftwareFeature>
      gcm_supported_software_features_;
  std::unique_ptr<base::OneShotTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceReenroller);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_DEVICE_REENROLLER_H_
