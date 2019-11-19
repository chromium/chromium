// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info.h"

#include "base/values.h"

namespace syncer {

bool DeviceInfo::SharingTargetInfo::operator==(
    const SharingTargetInfo& other) const {
  return fcm_token == other.fcm_token && p256dh == other.p256dh &&
         auth_secret == other.auth_secret;
}

DeviceInfo::SharingInfo::SharingInfo(
    SharingTargetInfo vapid_target_info,
    SharingTargetInfo sender_id_target_info,
    std::set<sync_pb::SharingSpecificFields::EnabledFeatures> enabled_features)
    : vapid_target_info(std::move(vapid_target_info)),
      sender_id_target_info(std::move(sender_id_target_info)),
      enabled_features(std::move(enabled_features)) {}

DeviceInfo::SharingInfo::SharingInfo(const SharingInfo& other) = default;

DeviceInfo::SharingInfo::SharingInfo(SharingInfo&& other) = default;

DeviceInfo::SharingInfo& DeviceInfo::SharingInfo::operator=(
    const SharingInfo& other) = default;

DeviceInfo::SharingInfo::~SharingInfo() = default;

bool DeviceInfo::SharingInfo::operator==(const SharingInfo& other) const {
  return vapid_target_info == other.vapid_target_info &&
         sender_id_target_info == other.sender_id_target_info &&
         enabled_features == other.enabled_features;
}

DeviceInfo::DeviceInfo(const std::string& guid,
                       const std::string& client_name,
                       const std::string& chrome_version,
                       const std::string& sync_user_agent,
                       const sync_pb::SyncEnums::DeviceType device_type,
                       const std::string& signin_scoped_device_id,
                       const base::SysInfo::HardwareInfo& hardware_info,
                       base::Time last_updated_timestamp,
                       bool send_tab_to_self_receiving_enabled,
                       const base::Optional<SharingInfo>& sharing_info)
    : guid_(guid),
      client_name_(client_name),
      chrome_version_(chrome_version),
      sync_user_agent_(sync_user_agent),
      device_type_(device_type),
      signin_scoped_device_id_(signin_scoped_device_id),
      hardware_info_(hardware_info),
      last_updated_timestamp_(last_updated_timestamp),
      send_tab_to_self_receiving_enabled_(send_tab_to_self_receiving_enabled),
      sharing_info_(sharing_info) {
  // We do not store device's serial number in DeviceInfo.
  hardware_info_.serial_number.clear();
}

DeviceInfo::~DeviceInfo() {}

const std::string& DeviceInfo::guid() const {
  return guid_;
}

const std::string& DeviceInfo::client_name() const {
  return client_name_;
}

const std::string& DeviceInfo::chrome_version() const {
  return chrome_version_;
}

const std::string& DeviceInfo::sync_user_agent() const {
  return sync_user_agent_;
}

const std::string& DeviceInfo::public_id() const {
  return public_id_;
}

sync_pb::SyncEnums::DeviceType DeviceInfo::device_type() const {
  return device_type_;
}

const std::string& DeviceInfo::signin_scoped_device_id() const {
  return signin_scoped_device_id_;
}

const base::SysInfo::HardwareInfo& DeviceInfo::hardware_info() const {
  return hardware_info_;
}

base::Time DeviceInfo::last_updated_timestamp() const {
  return last_updated_timestamp_;
}

bool DeviceInfo::send_tab_to_self_receiving_enabled() const {
  return send_tab_to_self_receiving_enabled_;
}

const base::Optional<DeviceInfo::SharingInfo>& DeviceInfo::sharing_info()
    const {
  return sharing_info_;
}

std::string DeviceInfo::GetOSString() const {
  switch (device_type_) {
    case sync_pb::SyncEnums_DeviceType_TYPE_WIN:
      return "win";
    case sync_pb::SyncEnums_DeviceType_TYPE_MAC:
      return "mac";
    case sync_pb::SyncEnums_DeviceType_TYPE_LINUX:
      return "linux";
    case sync_pb::SyncEnums_DeviceType_TYPE_CROS:
      return "chrome_os";
    case sync_pb::SyncEnums_DeviceType_TYPE_PHONE:
    case sync_pb::SyncEnums_DeviceType_TYPE_TABLET:
      // TODO(lipalani): crbug.com/170375. Add support for ios
      // phones and tablets.
      return "android";
    default:
      return "unknown";
  }
}

std::string DeviceInfo::GetDeviceTypeString() const {
  switch (device_type_) {
    case sync_pb::SyncEnums_DeviceType_TYPE_WIN:
    case sync_pb::SyncEnums_DeviceType_TYPE_MAC:
    case sync_pb::SyncEnums_DeviceType_TYPE_LINUX:
    case sync_pb::SyncEnums_DeviceType_TYPE_CROS:
      return "desktop_or_laptop";
    case sync_pb::SyncEnums_DeviceType_TYPE_PHONE:
      return "phone";
    case sync_pb::SyncEnums_DeviceType_TYPE_TABLET:
      return "tablet";
    default:
      return "unknown";
  }
}

bool DeviceInfo::Equals(const DeviceInfo& other) const {
  return this->guid() == other.guid() &&
         this->client_name() == other.client_name() &&
         this->chrome_version() == other.chrome_version() &&
         this->sync_user_agent() == other.sync_user_agent() &&
         this->device_type() == other.device_type() &&
         this->signin_scoped_device_id() == other.signin_scoped_device_id() &&
         this->hardware_info() == other.hardware_info() &&
         this->send_tab_to_self_receiving_enabled() ==
             other.send_tab_to_self_receiving_enabled() &&
         this->sharing_info() == other.sharing_info();
}

std::unique_ptr<base::DictionaryValue> DeviceInfo::ToValue() {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString("name", client_name_);
  value->SetString("id", public_id_);
  value->SetString("os", GetOSString());
  value->SetString("type", GetDeviceTypeString());
  value->SetString("chromeVersion", chrome_version_);
  value->SetInteger("lastUpdatedTimestamp", last_updated_timestamp().ToTimeT());
  value->SetBoolean("sendTabToSelfReceivingEnabled",
                    send_tab_to_self_receiving_enabled());
  value->SetBoolean("hasSharingInfo", sharing_info().has_value());
  return value;
}

void DeviceInfo::set_public_id(const std::string& id) {
  public_id_ = id;
}

void DeviceInfo::set_send_tab_to_self_receiving_enabled(bool new_value) {
  send_tab_to_self_receiving_enabled_ = new_value;
}

void DeviceInfo::set_sharing_info(
    const base::Optional<SharingInfo>& sharing_info) {
  sharing_info_ = sharing_info;
}

void DeviceInfo::set_client_name(const std::string& client_name) {
  client_name_ = client_name;
}

}  // namespace syncer
