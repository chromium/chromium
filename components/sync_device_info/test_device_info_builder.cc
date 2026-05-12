// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/test_device_info_builder.h"

#include "base/notreached.h"
#include "base/time/time.h"

namespace syncer {

namespace {

DeviceInfo::FormFactor DeriveDefaultFormFactor(DeviceInfo::OsType os_type) {
  switch (os_type) {
    case DeviceInfo::OsType::kWindows:
    case DeviceInfo::OsType::kMac:
    case DeviceInfo::OsType::kLinux:
    case DeviceInfo::OsType::kChromeOsAsh:
    case DeviceInfo::OsType::kChromeOsLacros:
      return DeviceInfo::FormFactor::kDesktop;
    case DeviceInfo::OsType::kAndroid:
    case DeviceInfo::OsType::kIOS:
      return DeviceInfo::FormFactor::kPhone;
    case DeviceInfo::OsType::kFuchsia:
    case DeviceInfo::OsType::kUnknown:
      return DeviceInfo::FormFactor::kUnknown;
  }
  NOTREACHED();
}

DeviceInfo::DeviceType DeriveDefaultDeviceType(DeviceInfo::OsType os_type) {
  switch (os_type) {
    case DeviceInfo::OsType::kWindows:
      return DeviceInfo::DeviceType::kWindows;
    case DeviceInfo::OsType::kMac:
      return DeviceInfo::DeviceType::kMac;
    case DeviceInfo::OsType::kLinux:
      return DeviceInfo::DeviceType::kLinux;
    case DeviceInfo::OsType::kChromeOsAsh:
    case DeviceInfo::OsType::kChromeOsLacros:
      return DeviceInfo::DeviceType::kChromeOS;
    case DeviceInfo::OsType::kAndroid:
    case DeviceInfo::OsType::kIOS:
      return DeviceInfo::DeviceType::kPhone;
    case DeviceInfo::OsType::kFuchsia:
    case DeviceInfo::OsType::kUnknown:
      return DeviceInfo::DeviceType::kOther;
  }
  NOTREACHED();
}

}  // namespace

TestDeviceInfoBuilder::TestDeviceInfoBuilder(DeviceInfo::OsType os_type)
    : device_type_(DeriveDefaultDeviceType(os_type)),
      os_type_(os_type),
      form_factor_(DeriveDefaultFormFactor(os_type)) {}

TestDeviceInfoBuilder::~TestDeviceInfoBuilder() = default;

TestDeviceInfoBuilder::TestDeviceInfoBuilder(TestDeviceInfoBuilder&&) = default;
TestDeviceInfoBuilder& TestDeviceInfoBuilder::operator=(
    TestDeviceInfoBuilder&&) = default;

std::unique_ptr<DeviceInfo> TestDeviceInfoBuilder::Build() const {
  return std::make_unique<DeviceInfo>(
      guid_, client_name_, chrome_version_, sync_user_agent_, device_type_,
      os_type_, form_factor_, signin_scoped_device_id_, manufacturer_name_,
      model_name_, full_hardware_class_, last_updated_timestamp_,
      pulse_interval_, send_tab_to_self_receiving_enabled_,
      send_tab_to_self_receiving_type_, sharing_info_, paask_info_,
      fcm_registration_token_, interested_data_types_,
      auto_sign_out_last_signin_timestamp_,
      desktop_to_ios_promo_receiving_enabled_,
      desktop_to_ios_promo_receiving_types_,
      glic_experimental_triggering_state_);
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithGuid(
    const std::string& guid) {
  guid_ = guid;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithClientName(
    const std::string& client_name) {
  client_name_ = client_name;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithChromeVersion(
    const std::string& chrome_version) {
  chrome_version_ = chrome_version;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithSyncUserAgent(
    const std::string& sync_user_agent) {
  sync_user_agent_ = sync_user_agent;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithDeviceType(
    DeviceInfo::DeviceType device_type) {
  device_type_ = device_type;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithOsType(
    DeviceInfo::OsType os_type) {
  os_type_ = os_type;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithFormFactor(
    DeviceInfo::FormFactor form_factor) {
  form_factor_ = form_factor;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithSigninScopedDeviceId(
    const std::string& signin_scoped_device_id) {
  signin_scoped_device_id_ = signin_scoped_device_id;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithManufacturerName(
    const std::string& manufacturer_name) {
  manufacturer_name_ = manufacturer_name;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithModelName(
    const std::string& model_name) {
  model_name_ = model_name;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithFullHardwareClass(
    const std::string& full_hardware_class) {
  full_hardware_class_ = full_hardware_class;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithLastUpdatedTimestamp(
    base::Time last_updated_timestamp) {
  last_updated_timestamp_ = last_updated_timestamp;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithPulseInterval(
    base::TimeDelta pulse_interval) {
  pulse_interval_ = pulse_interval;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithSendTabToSelfReceivingEnabled(
    bool send_tab_to_self_receiving_enabled) {
  send_tab_to_self_receiving_enabled_ = send_tab_to_self_receiving_enabled;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithSendTabToSelfReceivingType(
    DeviceInfo::SendTabReceivingType send_tab_to_self_receiving_type) {
  send_tab_to_self_receiving_type_ = send_tab_to_self_receiving_type;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithSharingInfo(
    const std::optional<DeviceInfo::SharingInfo>& sharing_info) {
  sharing_info_ = sharing_info;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithSharingInfo(
    const DeviceInfo::SharingInfo& sharing_info) {
  sharing_info_ = sharing_info;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithPaaskInfo(
    const std::optional<DeviceInfo::PhoneAsASecurityKeyInfo>& paask_info) {
  paask_info_ = paask_info;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithFcmRegistrationToken(
    const std::string& fcm_registration_token) {
  fcm_registration_token_ = fcm_registration_token;
  return *this;
}

TestDeviceInfoBuilder& TestDeviceInfoBuilder::WithInterestedDataTypes(
    const DataTypeSet& interested_data_types) {
  interested_data_types_ = interested_data_types;
  return *this;
}

TestDeviceInfoBuilder&
TestDeviceInfoBuilder::WithAutoSignOutLastSigninTimestamp(
    std::optional<base::Time> auto_sign_out_last_signin_timestamp) {
  auto_sign_out_last_signin_timestamp_ = auto_sign_out_last_signin_timestamp;
  return *this;
}

TestDeviceInfoBuilder&
TestDeviceInfoBuilder::WithDesktopToIosPromoReceivingEnabled(
    bool desktop_to_ios_promo_receiving_enabled) {
  desktop_to_ios_promo_receiving_enabled_ =
      desktop_to_ios_promo_receiving_enabled;
  return *this;
}

TestDeviceInfoBuilder&
TestDeviceInfoBuilder::WithDesktopToIosPromoReceivingTypes(
    const MobilePromoOnDesktopPromoTypeSet&
        desktop_to_ios_promo_receiving_types) {
  desktop_to_ios_promo_receiving_types_ = desktop_to_ios_promo_receiving_types;
  return *this;
}

TestDeviceInfoBuilder&
TestDeviceInfoBuilder::WithGlicExperimentalTriggeringState(
    DeviceInfo::GlicExperimentalTriggeringState
        glic_experimental_triggering_state) {
  glic_experimental_triggering_state_ = glic_experimental_triggering_state;
  return *this;
}

}  // namespace syncer
