// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/fake_device_info.h"

#include <optional>

#include "components/sync_device_info/device_info_util.h"

std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
    const std::string& guid,
    const std::string& name,
    const std::optional<syncer::DeviceInfo::SharingInfo>& sharing_info,
    sync_pb::SyncEnums_DeviceType device_type,
    syncer::DeviceInfo::OsType os_type,
    syncer::DeviceInfo::FormFactor form_factor,
    const std::string& manufacturer_name,
    const std::string& model_name,
    const std::string& full_hardware_class,
    base::Time last_updated_timestamp) {
  return std::make_unique<syncer::DeviceInfo>(
      guid, name, "chrome_version", "user_agent", device_type, os_type,
      form_factor, "device_id", manufacturer_name, model_name,
      full_hardware_class, last_updated_timestamp,
      syncer::DeviceInfoUtil::GetPulseInterval(),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      sharing_info,
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/syncer::DataTypeSet(),
      /*floating_workspace_last_signin_timestamp=*/std::nullopt);
}
