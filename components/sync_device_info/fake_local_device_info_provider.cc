// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/fake_local_device_info_provider.h"

#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_util.h"

namespace syncer {

FakeLocalDeviceInfoProvider::FakeLocalDeviceInfoProvider()
    : device_info_(
          "id",
          "name",
          "chrome_version",
          "user_agent",
          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
          DeviceInfo::OsType::kLinux,
          DeviceInfo::FormFactor::kDesktop,
          "device_id",
          "fake_manufacturer",
          "fake_model",
          "fake_full_hardware_class",
          /*last_updated_timestamp=*/base::Time::Now(),
          DeviceInfoUtil::GetPulseInterval(),
          /*send_tab_to_self_receiving_enabled=*/
          false,
          /*send_tab_to_self_receiving_type=*/
          sync_pb::
              SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
          /*sharing_info=*/std::nullopt,
          /*paask_info=*/std::nullopt,
          /*fcm_registration_token=*/std::string(),
          /*interested_data_types=*/DataTypeSet(),
          /*floating_workspace_last_signin_timestamp=*/std::nullopt) {}

FakeLocalDeviceInfoProvider::~FakeLocalDeviceInfoProvider() = default;

version_info::Channel FakeLocalDeviceInfoProvider::GetChannel() const {
  NOTIMPLEMENTED();
  return version_info::Channel::UNKNOWN;
}

const DeviceInfo* FakeLocalDeviceInfoProvider::GetLocalDeviceInfo() const {
  return ready_ ? &device_info_ : nullptr;
}

base::CallbackListSubscription
FakeLocalDeviceInfoProvider::RegisterOnInitializedCallback(
    const base::RepeatingClosure& callback) {
  return closure_list_.Add(callback);
}

void FakeLocalDeviceInfoProvider::SetReady(bool ready) {
  bool got_ready = !ready_ && ready;
  ready_ = ready;
  if (got_ready) {
    closure_list_.Notify();
  }
}

DeviceInfo* FakeLocalDeviceInfoProvider::GetMutableDeviceInfo() {
  return &device_info_;
}

}  // namespace syncer
