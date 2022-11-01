// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/cpp/fake_gcm_device_info_provider.h"

namespace ash {

namespace device_sync {

FakeGcmDeviceInfoProvider::FakeGcmDeviceInfoProvider(
    const cryptauth::GcmDeviceInfo& gcm_device_info)
    : gcm_device_info_(gcm_device_info) {}

FakeGcmDeviceInfoProvider::~FakeGcmDeviceInfoProvider() = default;

const cryptauth::GcmDeviceInfo& FakeGcmDeviceInfoProvider::GetGcmDeviceInfo()
    const {
  return gcm_device_info_;
}

}  // namespace device_sync

}  // namespace ash
