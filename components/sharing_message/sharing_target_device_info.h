// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_TARGET_DEVICE_INFO_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_TARGET_DEVICE_INFO_H_

#include "base/time/time.h"
#include "components/sync_device_info/device_info.h"

enum class SharingDevicePlatform;

// A class that holds information regarding the properties of a device.
class SharingTargetDeviceInfo {
 public:
  SharingTargetDeviceInfo(const std::string& guid,
                          const std::string& client_name,
                          SharingDevicePlatform platform,
                          base::TimeDelta pulse_interval,
                          syncer::DeviceInfo::FormFactor form_factor,
                          base::Time last_updated_timestamp);
  SharingTargetDeviceInfo(SharingTargetDeviceInfo&&);
  SharingTargetDeviceInfo(const SharingTargetDeviceInfo&) = delete;
  ~SharingTargetDeviceInfo();

  SharingTargetDeviceInfo& operator=(SharingTargetDeviceInfo&&);
  SharingTargetDeviceInfo& operator=(const SharingTargetDeviceInfo&) = delete;

  // Sync specific unique identifier for the device. Note if a device
  // is wiped and sync is set up again this id WILL be different.
  // The same device might have more than 1 guid if the device has multiple
  // accounts syncing.
  const std::string& guid() const { return guid_; }

  // The host name for the client.
  const std::string& client_name() const { return client_name_; }

  SharingDevicePlatform platform() const { return platform_; }

  base::TimeDelta pulse_interval() const { return pulse_interval_; }

  syncer::DeviceInfo::FormFactor form_factor() const { return form_factor_; }

  // Returns the time at which this device was last updated.
  base::Time last_updated_timestamp() const { return last_updated_timestamp_; }

 private:
  std::string guid_;
  std::string client_name_;
  SharingDevicePlatform platform_;
  base::TimeDelta pulse_interval_;
  syncer::DeviceInfo::FormFactor form_factor_;
  base::Time last_updated_timestamp_;
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_TARGET_DEVICE_INFO_H_
