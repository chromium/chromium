// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_proto_enum_util.h"

#include <utility>

#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

TEST(DeviceInfoProtoEnumUtilTest, DeriveOsFromDeviceType) {
  EXPECT_EQ(DeviceInfo::OsType::kChromeOsAsh,
            DeriveOsFromDeviceType(sync_pb::SyncEnums::TYPE_CROS, ""));
  EXPECT_EQ(DeviceInfo::OsType::kLinux,
            DeriveOsFromDeviceType(sync_pb::SyncEnums::TYPE_LINUX, ""));
  EXPECT_EQ(DeviceInfo::OsType::kMac,
            DeriveOsFromDeviceType(sync_pb::SyncEnums::TYPE_MAC, ""));
  EXPECT_EQ(DeviceInfo::OsType::kWindows,
            DeriveOsFromDeviceType(sync_pb::SyncEnums::TYPE_WIN, ""));
  EXPECT_EQ(
      DeviceInfo::OsType::kIOS,
      DeriveOsFromDeviceType(sync_pb::SyncEnums::TYPE_PHONE, "Apple Inc."));
  EXPECT_EQ(DeviceInfo::OsType::kAndroid,
            DeriveOsFromDeviceType(sync_pb::SyncEnums::TYPE_PHONE, "Google"));
  EXPECT_EQ(
      DeviceInfo::OsType::kIOS,
      DeriveOsFromDeviceType(sync_pb::SyncEnums::TYPE_TABLET, "Apple Inc."));
  EXPECT_EQ(DeviceInfo::OsType::kAndroid,
            DeriveOsFromDeviceType(sync_pb::SyncEnums::TYPE_TABLET, "Samsung"));
  EXPECT_EQ(DeviceInfo::OsType::kUnknown,
            DeriveOsFromDeviceType(sync_pb::SyncEnums::TYPE_UNSET, ""));
  EXPECT_EQ(DeviceInfo::OsType::kUnknown,
            DeriveOsFromDeviceType(sync_pb::SyncEnums::TYPE_OTHER, ""));
}

TEST(DeviceInfoProtoEnumUtilTest, DeriveFormFactorFromDeviceType) {
  EXPECT_EQ(DeviceInfo::FormFactor::kDesktop,
            DeriveFormFactorFromDeviceType(sync_pb::SyncEnums::TYPE_CROS));
  EXPECT_EQ(DeviceInfo::FormFactor::kDesktop,
            DeriveFormFactorFromDeviceType(sync_pb::SyncEnums::TYPE_LINUX));
  EXPECT_EQ(DeviceInfo::FormFactor::kDesktop,
            DeriveFormFactorFromDeviceType(sync_pb::SyncEnums::TYPE_MAC));
  EXPECT_EQ(DeviceInfo::FormFactor::kDesktop,
            DeriveFormFactorFromDeviceType(sync_pb::SyncEnums::TYPE_WIN));
  EXPECT_EQ(DeviceInfo::FormFactor::kPhone,
            DeriveFormFactorFromDeviceType(sync_pb::SyncEnums::TYPE_PHONE));
  EXPECT_EQ(DeviceInfo::FormFactor::kTablet,
            DeriveFormFactorFromDeviceType(sync_pb::SyncEnums::TYPE_TABLET));
  EXPECT_EQ(DeviceInfo::FormFactor::kUnknown,
            DeriveFormFactorFromDeviceType(sync_pb::SyncEnums::TYPE_UNSET));
  EXPECT_EQ(DeviceInfo::FormFactor::kUnknown,
            DeriveFormFactorFromDeviceType(sync_pb::SyncEnums::TYPE_OTHER));
}

TEST(DeviceInfoProtoEnumUtilTest, DeviceFormFactorConversion) {
  const std::pair<DeviceInfo::FormFactor, sync_pb::SyncEnums_DeviceFormFactor>
      kCases[] = {
          {DeviceInfo::FormFactor::kUnknown,
           sync_pb::SyncEnums::DEVICE_FORM_FACTOR_UNSPECIFIED},
          {DeviceInfo::FormFactor::kDesktop,
           sync_pb::SyncEnums::DEVICE_FORM_FACTOR_DESKTOP},
          {DeviceInfo::FormFactor::kPhone,
           sync_pb::SyncEnums::DEVICE_FORM_FACTOR_PHONE},
          {DeviceInfo::FormFactor::kTablet,
           sync_pb::SyncEnums::DEVICE_FORM_FACTOR_TABLET},
          {DeviceInfo::FormFactor::kAutomotive,
           sync_pb::SyncEnums::DEVICE_FORM_FACTOR_AUTOMOTIVE},
          {DeviceInfo::FormFactor::kWearable,
           sync_pb::SyncEnums::DEVICE_FORM_FACTOR_WEARABLE},
          {DeviceInfo::FormFactor::kTv,
           sync_pb::SyncEnums::DEVICE_FORM_FACTOR_TV},
      };

  for (const auto& [form_factor, proto_enum] : kCases) {
    EXPECT_EQ(form_factor, ToDeviceInfoFormFactor(proto_enum));
    EXPECT_EQ(proto_enum, ToDeviceFormFactorProto(form_factor));
  }
}

TEST(DeviceInfoProtoEnumUtilTest, OsTypeConversion) {
  const std::pair<DeviceInfo::OsType, sync_pb::SyncEnums_OsType> kCases[] = {
      {DeviceInfo::OsType::kUnknown, sync_pb::SyncEnums::OS_TYPE_UNSPECIFIED},
      {DeviceInfo::OsType::kWindows, sync_pb::SyncEnums::OS_TYPE_WINDOWS},
      {DeviceInfo::OsType::kMac, sync_pb::SyncEnums::OS_TYPE_MAC},
      {DeviceInfo::OsType::kLinux, sync_pb::SyncEnums::OS_TYPE_LINUX},
      {DeviceInfo::OsType::kChromeOsAsh,
       sync_pb::SyncEnums::OS_TYPE_CHROME_OS_ASH},
      {DeviceInfo::OsType::kAndroid, sync_pb::SyncEnums::OS_TYPE_ANDROID},
      {DeviceInfo::OsType::kIOS, sync_pb::SyncEnums::OS_TYPE_IOS},
      {DeviceInfo::OsType::kChromeOsLacros,
       sync_pb::SyncEnums::OS_TYPE_CHROME_OS_LACROS},
      {DeviceInfo::OsType::kFuchsia, sync_pb::SyncEnums::OS_TYPE_FUCHSIA},
  };

  for (const auto& [os_type, proto_enum] : kCases) {
    EXPECT_EQ(os_type, ToDeviceInfoOsType(proto_enum));
    EXPECT_EQ(proto_enum, ToOsTypeProto(os_type));
  }
}

TEST(DeviceInfoProtoEnumUtilTest, DeviceTypeConversion) {
  const std::pair<DeviceInfo::DeviceType, sync_pb::SyncEnums_DeviceType>
      kCases[] = {
          {DeviceInfo::DeviceType::kUnset,
           sync_pb::SyncEnums_DeviceType_TYPE_UNSET},
          {DeviceInfo::DeviceType::kWindows,
           sync_pb::SyncEnums_DeviceType_TYPE_WIN},
          {DeviceInfo::DeviceType::kMac,
           sync_pb::SyncEnums_DeviceType_TYPE_MAC},
          {DeviceInfo::DeviceType::kLinux,
           sync_pb::SyncEnums_DeviceType_TYPE_LINUX},
          {DeviceInfo::DeviceType::kChromeOS,
           sync_pb::SyncEnums_DeviceType_TYPE_CROS},
          {DeviceInfo::DeviceType::kOther,
           sync_pb::SyncEnums_DeviceType_TYPE_OTHER},
          {DeviceInfo::DeviceType::kPhone,
           sync_pb::SyncEnums_DeviceType_TYPE_PHONE},
          {DeviceInfo::DeviceType::kTablet,
           sync_pb::SyncEnums_DeviceType_TYPE_TABLET},
      };

  for (const auto& [device_type, proto_enum] : kCases) {
    EXPECT_EQ(device_type, ToDeviceInfoDeviceType(proto_enum));
    EXPECT_EQ(proto_enum, ToDeviceTypeProto(device_type));
  }
}

TEST(DeviceInfoProtoEnumUtilTest, SendTabReceivingTypeConversion) {
  const std::pair<DeviceInfo::SendTabReceivingType,
                  sync_pb::SyncEnums_SendTabReceivingType>
      kCases[] = {
          {DeviceInfo::SendTabReceivingType::kChromeOrUnspecified,
           sync_pb::
               SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED},
          {DeviceInfo::SendTabReceivingType::kChromeAndPushNotification,
           sync_pb::
               SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_AND_PUSH_NOTIFICATION},
      };

  for (const auto& [receiving_type, proto_enum] : kCases) {
    EXPECT_EQ(receiving_type, ToDeviceInfoSendTabReceivingType(proto_enum));
    EXPECT_EQ(proto_enum, ToSendTabReceivingTypeProto(receiving_type));
  }
}

TEST(DeviceInfoProtoEnumUtilTest, GlicExperimentalTriggeringStateConversion) {
  const std::pair<DeviceInfo::GlicExperimentalTriggeringState,
                  sync_pb::SyncEnums::GlicExperimentalTriggeringState>
      kCases[] = {
          {DeviceInfo::GlicExperimentalTriggeringState::kUnavailable,
           sync_pb::SyncEnums::UNAVAILABLE},
          {DeviceInfo::GlicExperimentalTriggeringState::kNeedsOptIn,
           sync_pb::SyncEnums::NEEDS_OPT_IN},
          {DeviceInfo::GlicExperimentalTriggeringState::kReady,
           sync_pb::SyncEnums::READY},
      };

  for (const auto& [state, proto_enum] : kCases) {
    EXPECT_EQ(state, ToDeviceInfoGlicExperimentalTriggeringState(proto_enum));
    EXPECT_EQ(proto_enum, ToGlicExperimentalTriggeringStateProto(state));
  }
}

TEST(DeviceInfoProtoEnumUtilTest, SharingFeatureConversion) {
  const std::pair<DeviceInfo::SharingFeature,
                  sync_pb::SharingSpecificFields_EnabledFeatures>
      kCases[] = {
          {DeviceInfo::SharingFeature::kUnknown,
           sync_pb::SharingSpecificFields::UNKNOWN},
          {DeviceInfo::SharingFeature::kSmsFetcher,
           sync_pb::SharingSpecificFields::SMS_FETCHER},
          {DeviceInfo::SharingFeature::kRemoteCopy,
           sync_pb::SharingSpecificFields::REMOTE_COPY},
          {DeviceInfo::SharingFeature::kOptimizationGuidePushNotification,
           sync_pb::SharingSpecificFields::
               OPTIMIZATION_GUIDE_PUSH_NOTIFICATION},
          {DeviceInfo::SharingFeature::kOneTimeTokenBackendNotification,
           sync_pb::SharingSpecificFields::ONE_TIME_TOKEN_BACKEND_NOTIFICATION},
          {DeviceInfo::SharingFeature::kGlicExperimentalTriggering,
           sync_pb::SharingSpecificFields::GLIC_EXPERIMENTAL_TRIGGERING},
          {DeviceInfo::SharingFeature::kBrowserActuator,
           sync_pb::SharingSpecificFields::BROWSER_ACTUATOR},
      };

  for (const auto& [feature, proto_enum] : kCases) {
    EXPECT_EQ(feature, ToDeviceInfoSharingFeature(proto_enum));
    EXPECT_EQ(proto_enum, ToSharingFeatureProto(feature));
  }
}

}  // namespace
}  // namespace syncer
