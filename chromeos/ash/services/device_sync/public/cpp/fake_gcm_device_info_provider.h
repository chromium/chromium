// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_GCM_DEVICE_INFO_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_GCM_DEVICE_INFO_PROVIDER_H_

#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/gcm_device_info_provider.h"

namespace ash {

namespace device_sync {

// Test GcmDeviceInfoProvider implementation.
class FakeGcmDeviceInfoProvider : public GcmDeviceInfoProvider {
 public:
  explicit FakeGcmDeviceInfoProvider(
      const cryptauth::GcmDeviceInfo& gcm_device_info);

  FakeGcmDeviceInfoProvider(const FakeGcmDeviceInfoProvider&) = delete;
  FakeGcmDeviceInfoProvider& operator=(const FakeGcmDeviceInfoProvider&) =
      delete;

  ~FakeGcmDeviceInfoProvider() override;

  // GcmDeviceInfoProvider:
  const cryptauth::GcmDeviceInfo& GetGcmDeviceInfo() const override;

 private:
  const cryptauth::GcmDeviceInfo gcm_device_info_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_GCM_DEVICE_INFO_PROVIDER_H_
