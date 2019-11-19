// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/fake_local_device_info_provider.h"

#include "base/system/sys_info.h"
#include "base/time/time.h"

namespace syncer {

FakeLocalDeviceInfoProvider::FakeLocalDeviceInfoProvider()
    : device_info_(
          "id",
          "name",
          "chrome_version",
          "user_agent",
          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
          "device_id",
          base::SysInfo::HardwareInfo{"manufacturer", "model", "serial"},
          /*last_updated_timestamp=*/base::Time::Now(),
          /*send_tab_to_self_receiving_enabled=*/false,
          /*sharing_info=*/base::nullopt) {}

FakeLocalDeviceInfoProvider::~FakeLocalDeviceInfoProvider() = default;

version_info::Channel FakeLocalDeviceInfoProvider::GetChannel() const {
  NOTIMPLEMENTED();
  return version_info::Channel::UNKNOWN;
}

const DeviceInfo* FakeLocalDeviceInfoProvider::GetLocalDeviceInfo() const {
  return ready_ ? &device_info_ : nullptr;
}

std::unique_ptr<LocalDeviceInfoProvider::Subscription>
FakeLocalDeviceInfoProvider::RegisterOnInitializedCallback(
    const base::RepeatingClosure& callback) {
  return callback_list_.Add(callback);
}

void FakeLocalDeviceInfoProvider::SetReady(bool ready) {
  bool got_ready = !ready_ && ready;
  ready_ = ready;
  if (got_ready)
    callback_list_.Notify();
}

DeviceInfo* FakeLocalDeviceInfoProvider::GetMutableDeviceInfo() {
  return &device_info_;
}

}  // namespace syncer
