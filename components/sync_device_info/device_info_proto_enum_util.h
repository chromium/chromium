// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_PROTO_ENUM_UTIL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_PROTO_ENUM_UTIL_H_

#include <string>

#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"

namespace syncer {

DeviceInfo::OsType DeriveOsFromDeviceType(
    const sync_pb::SyncEnums_DeviceType& device_type,
    const std::string& manufacturer_name);

DeviceInfo::FormFactor DeriveFormFactorFromDeviceType(
    const sync_pb::SyncEnums_DeviceType& device_type);

syncer::DeviceInfo::FormFactor ToDeviceInfoFormFactor(
    const sync_pb::SyncEnums_DeviceFormFactor& form_factor);

sync_pb::SyncEnums_DeviceFormFactor ToDeviceFormFactorProto(
    const syncer::DeviceInfo::FormFactor& form_factor);

syncer::DeviceInfo::OsType ToDeviceInfoOsType(
    const sync_pb::SyncEnums_OsType& os_type);

sync_pb::SyncEnums_OsType ToOsTypeProto(const DeviceInfo::OsType& os_type);

// Conversion functions for DeviceInfo::DeviceType <-> proto.
DeviceInfo::DeviceType ToDeviceInfoDeviceType(
    sync_pb::SyncEnums_DeviceType device_type);
sync_pb::SyncEnums_DeviceType ToDeviceTypeProto(
    DeviceInfo::DeviceType device_type);

// Conversion functions for DeviceInfo::SendTabReceivingType <-> proto.
DeviceInfo::SendTabReceivingType ToDeviceInfoSendTabReceivingType(
    sync_pb::SyncEnums_SendTabReceivingType type);
sync_pb::SyncEnums_SendTabReceivingType ToSendTabReceivingTypeProto(
    DeviceInfo::SendTabReceivingType type);

// Conversion functions for DeviceInfo::GlicExperimentalTriggeringState <->
// proto.
DeviceInfo::GlicExperimentalTriggeringState
ToDeviceInfoGlicExperimentalTriggeringState(
    sync_pb::SyncEnums::GlicExperimentalTriggeringState state);
sync_pb::SyncEnums::GlicExperimentalTriggeringState
ToGlicExperimentalTriggeringStateProto(
    DeviceInfo::GlicExperimentalTriggeringState state);

// Conversion functions for DeviceInfo::SharingFeature <-> proto.
DeviceInfo::SharingFeature ToDeviceInfoSharingFeature(
    sync_pb::SharingSpecificFields_EnabledFeatures feature);
sync_pb::SharingSpecificFields_EnabledFeatures ToSharingFeatureProto(
    DeviceInfo::SharingFeature feature);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_PROTO_ENUM_UTIL_H_
