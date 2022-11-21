// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_proto_enum_util.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace syncer {

DeviceInfo::OsType DeriveOsFromDeviceType(
    const sync_pb::SyncEnums_DeviceType& device_type,
    const std::string& manufacturer_name) {
  switch (device_type) {
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_CROS:
      return DeviceInfo::OsType::kChromeOsAsh;
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_LINUX:
      return DeviceInfo::OsType::kLinux;
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_MAC:
      return DeviceInfo::OsType::kMac;
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_WIN:
      return DeviceInfo::OsType::kWindows;
    case sync_pb::SyncEnums_DeviceType_TYPE_PHONE:
    case sync_pb::SyncEnums_DeviceType_TYPE_TABLET:
      if (manufacturer_name == "Apple Inc.")
        return DeviceInfo::OsType::kIOS;
      return DeviceInfo::OsType::kAndroid;
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_UNSET:
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_OTHER:
      return DeviceInfo::OsType::kUnknown;
  }
}

DeviceInfo::FormFactor DeriveFormFactorFromDeviceType(
    const sync_pb::SyncEnums_DeviceType& device_type) {
  switch (device_type) {
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_CROS:
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_LINUX:
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_MAC:
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_WIN:
      return DeviceInfo::FormFactor::kDesktop;
    case sync_pb::SyncEnums_DeviceType_TYPE_PHONE:
      return DeviceInfo::FormFactor::kPhone;
    case sync_pb::SyncEnums_DeviceType_TYPE_TABLET:
      return DeviceInfo::FormFactor::kTablet;
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_UNSET:
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_OTHER:
      return DeviceInfo::FormFactor::kUnknown;
  }
}

syncer::DeviceInfo::FormFactor ToDeviceInfoFormFactor(
    const sync_pb::SyncEnums_DeviceFormFactor& form_factor) {
  switch (form_factor) {
    case sync_pb::SyncEnums::DeviceFormFactor::
        SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED:
      return syncer::DeviceInfo::FormFactor::kUnknown;
    case sync_pb::SyncEnums::DeviceFormFactor::
        SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP:
      return syncer::DeviceInfo::FormFactor::kDesktop;
    case sync_pb::SyncEnums::DeviceFormFactor::
        SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE:
      return syncer::DeviceInfo::FormFactor::kPhone;
    case sync_pb::SyncEnums::DeviceFormFactor::
        SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET:
      return syncer::DeviceInfo::FormFactor::kTablet;
  }
}

sync_pb::SyncEnums_DeviceFormFactor ToDeviceFormFactorProto(
    const syncer::DeviceInfo::FormFactor& form_factor) {
  switch (form_factor) {
    case syncer::DeviceInfo::FormFactor::kUnknown:
      return sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED;
    case syncer::DeviceInfo::FormFactor::kDesktop:
      return sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP;
    case syncer::DeviceInfo::FormFactor::kPhone:
      return sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;
    case syncer::DeviceInfo::FormFactor::kTablet:
      return sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET;
  }
}

}  // namespace syncer
