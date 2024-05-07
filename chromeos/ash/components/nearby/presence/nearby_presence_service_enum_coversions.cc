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

NearbyPresenceService::StatusCode ConvertToPresenceStatus(
    mojo_base::mojom::AbslStatusCode status_code) {
  switch (status_code) {
    case mojo_base::mojom::AbslStatusCode::kOk:
      return NearbyPresenceService::StatusCode::kAbslOk;
    case mojo_base::mojom::AbslStatusCode::kCancelled:
      return NearbyPresenceService::StatusCode::kAbslCancelled;
    case mojo_base::mojom::AbslStatusCode::kUnknown:
      return NearbyPresenceService::StatusCode::kAbslUnknown;
    case mojo_base::mojom::AbslStatusCode::kInvalidArgument:
      return NearbyPresenceService::StatusCode::kAbslInvalidArgument;
    case mojo_base::mojom::AbslStatusCode::kDeadlineExceeded:
      return NearbyPresenceService::StatusCode::kAbslDeadlineExceeded;
    case mojo_base::mojom::AbslStatusCode::kNotFound:
      return NearbyPresenceService::StatusCode::kAbslNotFound;
    case mojo_base::mojom::AbslStatusCode::kAlreadyExists:
      return NearbyPresenceService::StatusCode::kAbslAlreadyExists;
    case mojo_base::mojom::AbslStatusCode::kPermissionDenied:
      return NearbyPresenceService::StatusCode::kAbslPermissionDenied;
    case mojo_base::mojom::AbslStatusCode::kResourceExhausted:
      return NearbyPresenceService::StatusCode::kAbslResourceExhausted;
    case mojo_base::mojom::AbslStatusCode::kFailedPrecondition:
      return NearbyPresenceService::StatusCode::kAbslFailedPrecondition;
    case mojo_base::mojom::AbslStatusCode::kAborted:
      return NearbyPresenceService::StatusCode::kAbslAborted;
    case mojo_base::mojom::AbslStatusCode::kOutOfRange:
      return NearbyPresenceService::StatusCode::kAbslOutOfRange;
    case mojo_base::mojom::AbslStatusCode::kUnimplemented:
      return NearbyPresenceService::StatusCode::kAbslUnimplemented;
    case mojo_base::mojom::AbslStatusCode::kInternal:
      return NearbyPresenceService::StatusCode::kAbslInternal;
    case mojo_base::mojom::AbslStatusCode::kUnavailable:
      return NearbyPresenceService::StatusCode::kAbslUnavailable;
    case mojo_base::mojom::AbslStatusCode::kDataLoss:
      return NearbyPresenceService::StatusCode::kAbslDataLoss;
    case mojo_base::mojom::AbslStatusCode::kUnauthenticated:
      return NearbyPresenceService::StatusCode::kAbslUnauthenticated;
  }
}

}  // namespace ash::nearby::presence
