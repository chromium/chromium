// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_proto_enum_util.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"

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
    const sync_pb::SyncEnums::DeviceFormFactor& form_factor) {
  switch (form_factor) {
    case sync_pb::SyncEnums::DEVICE_FORM_FACTOR_UNSPECIFIED:
      return syncer::DeviceInfo::FormFactor::kUnknown;
    case sync_pb::SyncEnums::DEVICE_FORM_FACTOR_DESKTOP:
      return syncer::DeviceInfo::FormFactor::kDesktop;
    case sync_pb::SyncEnums::DEVICE_FORM_FACTOR_PHONE:
      return syncer::DeviceInfo::FormFactor::kPhone;
    case sync_pb::SyncEnums::DEVICE_FORM_FACTOR_TABLET:
      return syncer::DeviceInfo::FormFactor::kTablet;
    case sync_pb::SyncEnums::DEVICE_FORM_FACTOR_AUTOMOTIVE:
      return syncer::DeviceInfo::FormFactor::kAutomotive;
    case sync_pb::SyncEnums::DEVICE_FORM_FACTOR_WEARABLE:
      return syncer::DeviceInfo::FormFactor::kWearable;
    case sync_pb::SyncEnums::DEVICE_FORM_FACTOR_TV:
      return syncer::DeviceInfo::FormFactor::kTv;
  }
}

sync_pb::SyncEnums_DeviceFormFactor ToDeviceFormFactorProto(
    const syncer::DeviceInfo::FormFactor& form_factor) {
  switch (form_factor) {
    case syncer::DeviceInfo::FormFactor::kUnknown:
      return sync_pb::SyncEnums::DEVICE_FORM_FACTOR_UNSPECIFIED;
    case syncer::DeviceInfo::FormFactor::kDesktop:
      return sync_pb::SyncEnums::DEVICE_FORM_FACTOR_DESKTOP;
    case syncer::DeviceInfo::FormFactor::kPhone:
      return sync_pb::SyncEnums::DEVICE_FORM_FACTOR_PHONE;
    case syncer::DeviceInfo::FormFactor::kTablet:
      return sync_pb::SyncEnums::DEVICE_FORM_FACTOR_TABLET;
    case syncer::DeviceInfo::FormFactor::kAutomotive:
      return sync_pb::SyncEnums::DEVICE_FORM_FACTOR_AUTOMOTIVE;
    case syncer::DeviceInfo::FormFactor::kWearable:
      return sync_pb::SyncEnums::DEVICE_FORM_FACTOR_WEARABLE;
    case syncer::DeviceInfo::FormFactor::kTv:
      return sync_pb::SyncEnums::DEVICE_FORM_FACTOR_TV;
  }
}

syncer::DeviceInfo::OsType ToDeviceInfoOsType(
    const sync_pb::SyncEnums_OsType& os_type) {
  switch (os_type) {
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_UNSPECIFIED:
      return syncer::DeviceInfo::OsType::kUnknown;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_WINDOWS:
      return syncer::DeviceInfo::OsType::kWindows;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_MAC:
      return syncer::DeviceInfo::OsType::kMac;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_LINUX:
      return syncer::DeviceInfo::OsType::kLinux;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_CHROME_OS_ASH:
      return syncer::DeviceInfo::OsType::kChromeOsAsh;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_ANDROID:
      return syncer::DeviceInfo::OsType::kAndroid;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_IOS:
      return syncer::DeviceInfo::OsType::kIOS;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_CHROME_OS_LACROS:
      return syncer::DeviceInfo::OsType::kChromeOsLacros;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_FUCHSIA:
      return syncer::DeviceInfo::OsType::kFuchsia;
  }
}

sync_pb::SyncEnums_OsType ToOsTypeProto(const DeviceInfo::OsType& os_type) {
  switch (os_type) {
    case DeviceInfo::OsType::kUnknown:
      return sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_UNSPECIFIED;
    case DeviceInfo::OsType::kWindows:
      return sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_WINDOWS;
    case DeviceInfo::OsType::kMac:
      return sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_MAC;
    case DeviceInfo::OsType::kLinux:
      return sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_LINUX;
    case DeviceInfo::OsType::kChromeOsAsh:
      return sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_CHROME_OS_ASH;
    case DeviceInfo::OsType::kAndroid:
      return sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_ANDROID;
    case DeviceInfo::OsType::kIOS:
      return sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_IOS;
    case DeviceInfo::OsType::kChromeOsLacros:
      return sync_pb::SyncEnums::OsType::
          SyncEnums_OsType_OS_TYPE_CHROME_OS_LACROS;
    case DeviceInfo::OsType::kFuchsia:
      return sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_FUCHSIA;
  }
}

DeviceInfo::DeviceType ToDeviceInfoDeviceType(
    sync_pb::SyncEnums_DeviceType device_type) {
  switch (device_type) {
    case sync_pb::SyncEnums_DeviceType_TYPE_UNSET:
      return DeviceInfo::DeviceType::kUnset;
    case sync_pb::SyncEnums_DeviceType_TYPE_WIN:
      return DeviceInfo::DeviceType::kWindows;
    case sync_pb::SyncEnums_DeviceType_TYPE_MAC:
      return DeviceInfo::DeviceType::kMac;
    case sync_pb::SyncEnums_DeviceType_TYPE_LINUX:
      return DeviceInfo::DeviceType::kLinux;
    case sync_pb::SyncEnums_DeviceType_TYPE_CROS:
      return DeviceInfo::DeviceType::kChromeOS;
    case sync_pb::SyncEnums_DeviceType_TYPE_OTHER:
      return DeviceInfo::DeviceType::kOther;
    case sync_pb::SyncEnums_DeviceType_TYPE_PHONE:
      return DeviceInfo::DeviceType::kPhone;
    case sync_pb::SyncEnums_DeviceType_TYPE_TABLET:
      return DeviceInfo::DeviceType::kTablet;
  }
  NOTREACHED();
}

sync_pb::SyncEnums_DeviceType ToDeviceTypeProto(
    DeviceInfo::DeviceType device_type) {
  switch (device_type) {
    case DeviceInfo::DeviceType::kUnset:
      return sync_pb::SyncEnums_DeviceType_TYPE_UNSET;
    case DeviceInfo::DeviceType::kWindows:
      return sync_pb::SyncEnums_DeviceType_TYPE_WIN;
    case DeviceInfo::DeviceType::kMac:
      return sync_pb::SyncEnums_DeviceType_TYPE_MAC;
    case DeviceInfo::DeviceType::kLinux:
      return sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
    case DeviceInfo::DeviceType::kChromeOS:
      return sync_pb::SyncEnums_DeviceType_TYPE_CROS;
    case DeviceInfo::DeviceType::kOther:
      return sync_pb::SyncEnums_DeviceType_TYPE_OTHER;
    case DeviceInfo::DeviceType::kPhone:
      return sync_pb::SyncEnums_DeviceType_TYPE_PHONE;
    case DeviceInfo::DeviceType::kTablet:
      return sync_pb::SyncEnums_DeviceType_TYPE_TABLET;
  }
  NOTREACHED();
}

DeviceInfo::SendTabReceivingType ToDeviceInfoSendTabReceivingType(
    sync_pb::SyncEnums_SendTabReceivingType type) {
  switch (type) {
    case sync_pb::
        SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED:
      return DeviceInfo::SendTabReceivingType::kChromeOrUnspecified;
    case sync_pb::
        SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_AND_PUSH_NOTIFICATION:
      return DeviceInfo::SendTabReceivingType::kChromeAndPushNotification;
  }
  NOTREACHED();
}

sync_pb::SyncEnums_SendTabReceivingType ToSendTabReceivingTypeProto(
    DeviceInfo::SendTabReceivingType type) {
  switch (type) {
    case DeviceInfo::SendTabReceivingType::kChromeOrUnspecified:
      return sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED;
    case DeviceInfo::SendTabReceivingType::kChromeAndPushNotification:
      return sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_AND_PUSH_NOTIFICATION;
  }
  NOTREACHED();
}

DeviceInfo::SharingFeature ToDeviceInfoSharingFeature(
    sync_pb::SharingSpecificFields_EnabledFeatures feature) {
  switch (feature) {
    case sync_pb::SharingSpecificFields::UNKNOWN:
      return DeviceInfo::SharingFeature::kUnknown;
    case sync_pb::SharingSpecificFields::SMS_FETCHER:
      return DeviceInfo::SharingFeature::kSmsFetcher;
    case sync_pb::SharingSpecificFields::REMOTE_COPY:
      return DeviceInfo::SharingFeature::kRemoteCopy;
    case sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2:
      return DeviceInfo::SharingFeature::kClickToCallV2;
    case sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2:
      return DeviceInfo::SharingFeature::kSharedClipboardV2;
    case sync_pb::SharingSpecificFields::OPTIMIZATION_GUIDE_PUSH_NOTIFICATION:
      return DeviceInfo::SharingFeature::kOptimizationGuidePushNotification;
    case sync_pb::SharingSpecificFields::ONE_TIME_TOKEN_BACKEND_NOTIFICATION:
      return DeviceInfo::SharingFeature::kOneTimeTokenBackendNotification;
  }
  NOTREACHED();
}

sync_pb::SharingSpecificFields_EnabledFeatures ToSharingFeatureProto(
    DeviceInfo::SharingFeature feature) {
  switch (feature) {
    case DeviceInfo::SharingFeature::kUnknown:
      return sync_pb::SharingSpecificFields::UNKNOWN;
    case DeviceInfo::SharingFeature::kSmsFetcher:
      return sync_pb::SharingSpecificFields::SMS_FETCHER;
    case DeviceInfo::SharingFeature::kRemoteCopy:
      return sync_pb::SharingSpecificFields::REMOTE_COPY;
    case DeviceInfo::SharingFeature::kClickToCallV2:
      return sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2;
    case DeviceInfo::SharingFeature::kSharedClipboardV2:
      return sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2;
    case DeviceInfo::SharingFeature::kOptimizationGuidePushNotification:
      return sync_pb::SharingSpecificFields::
          OPTIMIZATION_GUIDE_PUSH_NOTIFICATION;
    case DeviceInfo::SharingFeature::kOneTimeTokenBackendNotification:
      return sync_pb::SharingSpecificFields::
          ONE_TIME_TOKEN_BACKEND_NOTIFICATION;
  }
  NOTREACHED();
}

}  // namespace syncer
