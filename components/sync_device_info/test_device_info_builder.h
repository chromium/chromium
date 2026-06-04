// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_TEST_DEVICE_INFO_BUILDER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_TEST_DEVICE_INFO_BUILDER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync_device_info/device_info.h"

namespace syncer {

// A builder to construct syncer::DeviceInfo objects for testing.
// It initializes with sensible default values, and allows overriding only
// the required properties.
class TestDeviceInfoBuilder {
 public:
  explicit TestDeviceInfoBuilder(
      DeviceInfo::OsType os_type = DeviceInfo::OsType::kWindows);
  explicit TestDeviceInfoBuilder(const DeviceInfo& other);
  ~TestDeviceInfoBuilder();

  // Disable copy
  TestDeviceInfoBuilder(const TestDeviceInfoBuilder&) = delete;
  TestDeviceInfoBuilder& operator=(const TestDeviceInfoBuilder&) = delete;

  // Move is okay
  TestDeviceInfoBuilder(TestDeviceInfoBuilder&&);
  TestDeviceInfoBuilder& operator=(TestDeviceInfoBuilder&&);

  std::unique_ptr<DeviceInfo> Build() const;

  TestDeviceInfoBuilder& WithGuid(const std::string& guid);
  TestDeviceInfoBuilder& WithClientName(const std::string& client_name);
  TestDeviceInfoBuilder& WithChromeVersion(const std::string& chrome_version);
  TestDeviceInfoBuilder& WithSyncUserAgent(const std::string& sync_user_agent);
  TestDeviceInfoBuilder& WithDeviceType(DeviceInfo::DeviceType device_type);
  TestDeviceInfoBuilder& WithOsType(DeviceInfo::OsType os_type);
  TestDeviceInfoBuilder& WithFormFactor(DeviceInfo::FormFactor form_factor);
  TestDeviceInfoBuilder& WithSigninScopedDeviceId(
      const std::string& signin_scoped_device_id);
  TestDeviceInfoBuilder& WithManufacturerName(
      const std::string& manufacturer_name);
  TestDeviceInfoBuilder& WithModelName(const std::string& model_name);
  TestDeviceInfoBuilder& WithFullHardwareClass(
      const std::string& full_hardware_class);
  TestDeviceInfoBuilder& WithAndroidBuildFingerprintPrefix(
      std::optional<std::string> android_os_build_fingerprint_prefix);
  TestDeviceInfoBuilder& WithLastUpdatedTimestamp(
      base::Time last_updated_timestamp);
  TestDeviceInfoBuilder& WithPulseInterval(base::TimeDelta pulse_interval);
  TestDeviceInfoBuilder& WithSendTabToSelfReceivingEnabled(
      bool send_tab_to_self_receiving_enabled);
  TestDeviceInfoBuilder& WithSendTabToSelfReceivingType(
      DeviceInfo::SendTabReceivingType send_tab_to_self_receiving_type);
  TestDeviceInfoBuilder& WithSharingInfo(
      const std::optional<DeviceInfo::SharingInfo>& sharing_info);
  TestDeviceInfoBuilder& WithSharingInfo(
      const DeviceInfo::SharingInfo& sharing_info);
  TestDeviceInfoBuilder& WithPaaskInfo(
      const std::optional<DeviceInfo::PhoneAsASecurityKeyInfo>& paask_info);
  TestDeviceInfoBuilder& WithFcmRegistrationToken(
      const std::string& fcm_registration_token);
  TestDeviceInfoBuilder& WithInterestedDataTypes(
      const DataTypeSet& interested_data_types);
  TestDeviceInfoBuilder& WithAutoSignOutLastSigninTimestamp(
      std::optional<base::Time> auto_sign_out_last_signin_timestamp);
  TestDeviceInfoBuilder& WithDesktopToIosPromoReceivingEnabled(
      bool desktop_to_ios_promo_receiving_enabled);
  TestDeviceInfoBuilder& WithDesktopToIosPromoReceivingTypes(
      const MobilePromoOnDesktopPromoTypeSet&
          desktop_to_ios_promo_receiving_types);
  TestDeviceInfoBuilder& WithGlicExperimentalTriggeringState(
      DeviceInfo::GlicExperimentalTriggeringState
          glic_experimental_triggering_state);
  TestDeviceInfoBuilder& WithGlicExperimentalTriggeringVersion(
      std::optional<int> glic_experimental_triggering_version);
  TestDeviceInfoBuilder& WithServerDeterminedModelName(
      const std::optional<std::string>& server_determined_model_name);

 private:
  std::string guid_ = "guid";
  std::string client_name_ = "client_name";
  std::string chrome_version_ = "chrome_version";
  std::string sync_user_agent_ = "sync_user_agent";
  DeviceInfo::DeviceType device_type_;
  DeviceInfo::OsType os_type_;
  DeviceInfo::FormFactor form_factor_;
  std::string signin_scoped_device_id_ = "signin_scoped_device_id";
  std::string manufacturer_name_ = "manufacturer";
  std::string model_name_ = "model";
  std::string full_hardware_class_;
  std::optional<std::string> android_os_build_fingerprint_prefix_;
  base::Time last_updated_timestamp_ = base::Time::Now();
  base::TimeDelta pulse_interval_ = base::Days(1);
  bool send_tab_to_self_receiving_enabled_ = false;
  DeviceInfo::SendTabReceivingType send_tab_to_self_receiving_type_ =
      DeviceInfo::SendTabReceivingType::kChromeOrUnspecified;
  std::optional<DeviceInfo::SharingInfo> sharing_info_;
  std::optional<DeviceInfo::PhoneAsASecurityKeyInfo> paask_info_;
  std::string fcm_registration_token_;
  DataTypeSet interested_data_types_ = DataTypeSet::All();
  std::optional<base::Time> auto_sign_out_last_signin_timestamp_;
  bool desktop_to_ios_promo_receiving_enabled_ = false;
  MobilePromoOnDesktopPromoTypeSet desktop_to_ios_promo_receiving_types_;
  DeviceInfo::GlicExperimentalTriggeringState
      glic_experimental_triggering_state_ =
          DeviceInfo::GlicExperimentalTriggeringState::kUnavailable;
  std::optional<int> glic_experimental_triggering_version_;
  std::optional<std::string> server_determined_model_name_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_TEST_DEVICE_INFO_BUILDER_H_
