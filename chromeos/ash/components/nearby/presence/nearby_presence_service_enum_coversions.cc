// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service_enum_coversions.h"

namespace ash::nearby::presence {

::nearby::internal::DeviceType ConvertMojomDeviceType(
    mojom::PresenceDeviceType mojom_type) {
  switch (mojom_type) {
    case mojom::PresenceDeviceType::kUnknown:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_UNKNOWN;
    case mojom::PresenceDeviceType::kPhone:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_PHONE;
    case mojom::PresenceDeviceType::kTablet:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_TABLET;
    case mojom::PresenceDeviceType::kDisplay:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_DISPLAY;
    case mojom::PresenceDeviceType::kLaptop:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_LAPTOP;
    case mojom::PresenceDeviceType::kTv:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_TV;
    case mojom::PresenceDeviceType::kWatch:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_WATCH;
    case mojom::PresenceDeviceType::kChromeos:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS;
    case mojom::PresenceDeviceType::kFoldable:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_FOLDABLE;
  }
}

NearbyPresenceService::PresenceIdentityType ConvertToMojomIdentityType(
    ::nearby::internal::IdentityType identity_type) {
  switch (identity_type) {
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED:
      return NearbyPresenceService::PresenceIdentityType::
          kIdentityTypeUnspecified;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP:
      return NearbyPresenceService::PresenceIdentityType::
          kIdentityTypePrivateGroup;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP:
      return NearbyPresenceService::PresenceIdentityType::
          kIdentityTypeContactsGroup;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC:
      return NearbyPresenceService::PresenceIdentityType::kIdentityTypePublic;
    default:
      return NearbyPresenceService::PresenceIdentityType::
          kIdentityTypeUnspecified;
  }
}

}  // namespace ash::nearby::presence
