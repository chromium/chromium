// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_presence_conversions.h"

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"

namespace ash::nearby::presence {

::nearby::internal::DeviceType DeviceTypeFromMojom(
    mojom::PresenceDeviceType device_type) {
  switch (device_type) {
    case mojom::PresenceDeviceType::kUnspecified:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_UNKNOWN;
    case mojom::PresenceDeviceType::kPhone:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_PHONE;
    case mojom::PresenceDeviceType::kTablet:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_TABLET;
    case mojom::PresenceDeviceType::kDisplay:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_DISPLAY;
    case mojom::PresenceDeviceType::kTv:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_TV;
    case mojom::PresenceDeviceType::kWatch:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_WATCH;
    case mojom::PresenceDeviceType::kChromeos:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS;
    case mojom::PresenceDeviceType::kLaptop:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_LAPTOP;
    case mojom::PresenceDeviceType::kFoldable:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_FOLDABLE;
  }
}

::nearby::internal::Metadata MetadataFromMojom(mojom::Metadata* metadata) {
  ::nearby::internal::Metadata proto;
  proto.set_device_type(DeviceTypeFromMojom(metadata->device_type));
  proto.set_account_name(metadata->account_name);
  proto.set_user_name(metadata->user_name);
  proto.set_device_name(metadata->device_name);
  proto.set_user_name(metadata->user_name);
  proto.set_device_profile_url(metadata->device_profile_url);
  proto.set_bluetooth_mac_address(
      std::string(metadata->bluetooth_mac_address.begin(),
                  metadata->bluetooth_mac_address.end()));
  return proto;
}

mojom::IdentityType IdentityTypeToMojom(
    ::nearby::internal::IdentityType identity_type) {
  switch (identity_type) {
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED:
      return mojom::IdentityType::kIdentityTypeUnspecified;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE:
      return mojom::IdentityType::kIdentityTypePrivate;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_TRUSTED:
      return mojom::IdentityType::kIdentityTypeTrusted;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC:
      return mojom::IdentityType::kIdentityTypePublic;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PROVISIONED:
      return mojom::IdentityType::kIdentityTypeProvisioned;
    default:
      return mojom::IdentityType::kIdentityTypeUnspecified;
  }
}

mojom::SharedCredentialPtr SharedCredentialToMojom(
    ::nearby::internal::SharedCredential shared_credential) {
  return mojom::SharedCredential::New(
      std::vector<uint8_t>(shared_credential.secret_id().begin(),
                           shared_credential.secret_id().end()),
      std::vector<uint8_t>(shared_credential.key_seed().begin(),
                           shared_credential.key_seed().end()),
      shared_credential.start_time_millis(),
      shared_credential.end_time_millis(),
      std::vector<uint8_t>(
          shared_credential.encrypted_metadata_bytes_v0().begin(),
          shared_credential.encrypted_metadata_bytes_v0().end()),
      std::vector<uint8_t>(
          shared_credential.metadata_encryption_key_unsigned_adv_tag().begin(),
          shared_credential.metadata_encryption_key_unsigned_adv_tag().end()),
      std::vector<uint8_t>(
          shared_credential.connection_signature_verification_key().begin(),
          shared_credential.connection_signature_verification_key().end()),
      std::vector<uint8_t>(
          shared_credential.advertisement_signature_verification_key().begin(),
          shared_credential.advertisement_signature_verification_key().end()),
      IdentityTypeToMojom(shared_credential.identity_type()),
      std::vector<uint8_t>(shared_credential.version().begin(),
                           shared_credential.version().end()));
}

}  // namespace ash::nearby::presence
