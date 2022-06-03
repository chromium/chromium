// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_GCM_DEVICE_INFO_PROVIDER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_GCM_DEVICE_INFO_PROVIDER_H_

#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/public/cpp/gcm_device_info_provider.h"

namespace chromeos {

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

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_GCM_DEVICE_INFO_PROVIDER_H_
