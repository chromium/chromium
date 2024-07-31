// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_FAKE_DEVICE_INFO_H_
#define COMPONENTS_SHARING_MESSAGE_FAKE_DEVICE_INFO_H_

#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"

namespace syncer {
class DeviceInfo;
}  // namespace syncer

std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
    const std::string& guid,
    const std::string& name = "name",
    const std::optional<syncer::DeviceInfo::SharingInfo>& sharing_info =
        std::nullopt,
    sync_pb::SyncEnums_DeviceType device_type =
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
    syncer::DeviceInfo::OsType os_type = syncer::DeviceInfo::OsType::kLinux,
    syncer::DeviceInfo::FormFactor form_factor =
        syncer::DeviceInfo::FormFactor::kDesktop,
    const std::string& manufacturer_name = "manufacturer",
    const std::string& model_name = "model",
    const std::string& full_hardware_class = std::string(),
    base::Time last_updated_timestamp = base::Time::Now());

#endif  // COMPONENTS_SHARING_MESSAGE_FAKE_DEVICE_INFO_H_
