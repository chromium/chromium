// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info.h"

#include <optional>
#include <utility>

#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"

namespace syncer {

bool DeviceInfo::SharingTargetInfo::operator==(
    const SharingTargetInfo& other) const {
  return fcm_token == other.fcm_token && p256dh == other.p256dh &&
         auth_secret == other.auth_secret;
}

DeviceInfo::SharingInfo::SharingInfo(
    SharingTargetInfo vapid_target_info,
    SharingTargetInfo sender_id_target_info,
    std::string chime_representative_target_id,
    std::set<sync_pb::SharingSpecificFields::EnabledFeatures> enabled_features)
    : vapid_target_info(std::move(vapid_target_info)),
      sender_id_target_info(std::move(sender_id_target_info)),
      chime_representative_target_id(std::move(chime_representative_target_id)),
      enabled_features(std::move(enabled_features)) {}

DeviceInfo::SharingInfo::SharingInfo(const SharingInfo& other) = default;

DeviceInfo::SharingInfo::SharingInfo(SharingInfo&& other) = default;

DeviceInfo::SharingInfo& DeviceInfo::SharingInfo::operator=(
    const SharingInfo& other) = default;

DeviceInfo::SharingInfo::~SharingInfo() = default;

bool DeviceInfo::SharingInfo::operator==(const SharingInfo& other) const {
  return vapid_target_info == other.vapid_target_info &&
         sender_id_target_info == other.sender_id_target_info &&
         chime_representative_target_id ==
             other.chime_representative_target_id &&
         enabled_features == other.enabled_features;
}

DeviceInfo::PhoneAsASecurityKeyInfo::PhoneAsASecurityKeyInfo() = default;
DeviceInfo::PhoneAsASecurityKeyInfo::PhoneAsASecurityKeyInfo(
    const DeviceInfo::PhoneAsASecurityKeyInfo& other) = default;
DeviceInfo::PhoneAsASecurityKeyInfo::PhoneAsASecurityKeyInfo(
    DeviceInfo::PhoneAsASecurityKeyInfo&& other) = default;
DeviceInfo::PhoneAsASecurityKeyInfo&
DeviceInfo::PhoneAsASecurityKeyInfo::operator=(
    const DeviceInfo::PhoneAsASecurityKeyInfo& other) = default;
DeviceInfo::PhoneAsASecurityKeyInfo::~PhoneAsASecurityKeyInfo() = default;

bool DeviceInfo::PhoneAsASecurityKeyInfo::NonRotatingFieldsEqual(
    const PhoneAsASecurityKeyInfo& other) const {
  // secret and id are deliberately not tested. This is because their values are
  // based on the current time, but they should not cause an upload of the
  // local device's DeviceInfo.
  return tunnel_server_domain == other.tunnel_server_domain &&
         contact_id == other.contact_id &&
         peer_public_key_x962 == other.peer_public_key_x962;
}

DeviceInfo::DeviceInfo(
    const std::string& guid,
    const std::string& client_name,
    const std::string& chrome_version,
    const std::string& sync_user_agent,
    const sync_pb::SyncEnums::DeviceType device_type,
    const OsType os_type,
    const FormFactor form_factor,
    const std::string& signin_scoped_device_id,
    const std::string& manufacturer_name,
    const std::string& model_name,
    const std::string& full_hardware_class,
    base::Time last_updated_timestamp,
    base::TimeDelta pulse_interval,
    bool send_tab_to_self_receiving_enabled,
    sync_pb::SyncEnums_SendTabReceivingType send_tab_to_self_receiving_type,
    const std::optional<SharingInfo>& sharing_info,
    const std::optional<PhoneAsASecurityKeyInfo>& paask_info,
    const std::string& fcm_registration_token,
    const DataTypeSet& interested_data_types,
    std::optional<base::Time> floating_workspace_last_signin_timestamp)
    : guid_(guid),
      client_name_(client_name),
      chrome_version_(chrome_version),
      sync_user_agent_(sync_user_agent),
      device_type_(device_type),
      os_type_(os_type),
      form_factor_(form_factor),
      signin_scoped_device_id_(signin_scoped_device_id),
      manufacturer_name_(manufacturer_name),
      model_name_(model_name),
      full_hardware_class_(full_hardware_class),
      last_updated_timestamp_(last_updated_timestamp),
      pulse_interval_(pulse_interval),
      send_tab_to_self_receiving_enabled_(send_tab_to_self_receiving_enabled),
      send_tab_to_self_receiving_type_(send_tab_to_self_receiving_type),
      sharing_info_(sharing_info),
      paask_info_(paask_info),
      fcm_registration_token_(fcm_registration_token),
      interested_data_types_(interested_data_types),
      floating_workspace_last_signin_timestamp_(
          floating_workspace_last_signin_timestamp) {}

DeviceInfo::~DeviceInfo() = default;

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

DeviceInfo::OsType DeviceInfo::os_type() const {
  return os_type_;
}

DeviceInfo::FormFactor DeviceInfo::form_factor() const {
  return form_factor_;
}

const std::string& DeviceInfo::signin_scoped_device_id() const {
  return signin_scoped_device_id_;
}

const std::string& DeviceInfo::manufacturer_name() const {
  return manufacturer_name_;
}

const std::string& DeviceInfo::model_name() const {
  return model_name_;
}

const std::string& DeviceInfo::full_hardware_class() const {
  return full_hardware_class_;
}

base::Time DeviceInfo::last_updated_timestamp() const {
  return last_updated_timestamp_;
}

base::TimeDelta DeviceInfo::pulse_interval() const {
  return pulse_interval_;
}

bool DeviceInfo::send_tab_to_self_receiving_enabled() const {
  return send_tab_to_self_receiving_enabled_;
}

sync_pb::SyncEnums_SendTabReceivingType
DeviceInfo::send_tab_to_self_receiving_type() const {
  return send_tab_to_self_receiving_type_;
}

const std::optional<DeviceInfo::SharingInfo>& DeviceInfo::sharing_info() const {
  return sharing_info_;
}

const std::optional<DeviceInfo::PhoneAsASecurityKeyInfo>&
DeviceInfo::paask_info() const {
  return paask_info_;
}

const std::string& DeviceInfo::fcm_registration_token() const {
  return fcm_registration_token_;
}

const DataTypeSet& DeviceInfo::interested_data_types() const {
  return interested_data_types_;
}

std::optional<base::Time> DeviceInfo::floating_workspace_last_signin_timestamp()
    const {
  return floating_workspace_last_signin_timestamp_;
}

void DeviceInfo::set_public_id(const std::string& id) {
  public_id_ = id;
}

void DeviceInfo::set_full_hardware_class(
    const std::string& full_hardware_class) {
  full_hardware_class_ = full_hardware_class;
}

void DeviceInfo::set_send_tab_to_self_receiving_enabled(bool new_value) {
  send_tab_to_self_receiving_enabled_ = new_value;
}

void DeviceInfo::set_send_tab_to_self_receiving_type(
    sync_pb::SyncEnums_SendTabReceivingType new_value) {
  send_tab_to_self_receiving_type_ = new_value;
}

void DeviceInfo::set_sharing_info(
    const std::optional<SharingInfo>& sharing_info) {
  sharing_info_ = sharing_info;
}

void DeviceInfo::set_paask_info(
    std::optional<PhoneAsASecurityKeyInfo>&& paask_info) {
  paask_info_ = std::move(paask_info);
}

void DeviceInfo::set_client_name(const std::string& client_name) {
  client_name_ = client_name;
}

void DeviceInfo::set_fcm_registration_token(const std::string& fcm_token) {
  fcm_registration_token_ = fcm_token;
}

void DeviceInfo::set_interested_data_types(const DataTypeSet& data_types) {
  interested_data_types_ = data_types;
}

void DeviceInfo::set_floating_workspace_last_signin_timestamp(
    std::optional<base::Time> time) {
  floating_workspace_last_signin_timestamp_ = time;
}

}  // namespace syncer
