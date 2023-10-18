// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_presence_conversions.h"

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "third_party/nearby/src/presence/data_element.h"

namespace ash::nearby::presence {

::nearby::internal::DeviceType DeviceTypeFromMojom(
    mojom::PresenceDeviceType device_type) {
  switch (device_type) {
    case mojom::PresenceDeviceType::kUnknown:
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

mojom::PresenceDeviceType DeviceTypeToMojom(
    ::nearby::internal::DeviceType device_type) {
  switch (device_type) {
    case ::nearby::internal::DeviceType::DEVICE_TYPE_UNKNOWN:
      return mojom::PresenceDeviceType::kUnknown;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_PHONE:
      return mojom::PresenceDeviceType::kPhone;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_TABLET:
      return mojom::PresenceDeviceType::kTablet;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_DISPLAY:
      return mojom::PresenceDeviceType::kDisplay;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_TV:
      return mojom::PresenceDeviceType::kTv;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_WATCH:
      return mojom::PresenceDeviceType::kWatch;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS:
      return mojom::PresenceDeviceType::kChromeos;
    default:
      return mojom::PresenceDeviceType::kUnknown;
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

mojom::MetadataPtr MetadataToMojom(::nearby::internal::Metadata metadata) {
  return mojom::Metadata::New(
      DeviceTypeToMojom(metadata.device_type()), metadata.account_name(),
      metadata.device_name(), metadata.user_name(),
      metadata.device_profile_url(),
      std::vector<uint8_t>(metadata.bluetooth_mac_address().begin(),
                           metadata.bluetooth_mac_address().end()));
}

mojom::IdentityType ConvertIdentityTypeToMojom(
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

::nearby::internal::IdentityType ConvertMojomIdentityType(
    mojom::IdentityType identity_type) {
  switch (identity_type) {
    case mojom::IdentityType::kIdentityTypeUnspecified:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
    case mojom::IdentityType::kIdentityTypePrivate:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE;
    case mojom::IdentityType::kIdentityTypeTrusted:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_TRUSTED;
    case mojom::IdentityType::kIdentityTypePublic:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC;
    case mojom::IdentityType::kIdentityTypeProvisioned:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PROVISIONED;
    default:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
  }
}
mojo_base::mojom::AbslStatusCode CovertStatusToMojomStatus(
    absl::Status status) {
  switch (status.code()) {
    case absl::StatusCode::kOk:
      return mojo_base::mojom::AbslStatusCode::kOk;
    case absl::StatusCode::kCancelled:
      return mojo_base::mojom::AbslStatusCode::kCancelled;
    case absl::StatusCode::kUnknown:
      return mojo_base::mojom::AbslStatusCode::kUnknown;
    case absl::StatusCode::kInvalidArgument:
      return mojo_base::mojom::AbslStatusCode::kInvalidArgument;
    case absl::StatusCode::kDeadlineExceeded:
      return mojo_base::mojom::AbslStatusCode::kDeadlineExceeded;
    case absl::StatusCode::kNotFound:
      return mojo_base::mojom::AbslStatusCode::kNotFound;
    case absl::StatusCode::kAlreadyExists:
      return mojo_base::mojom::AbslStatusCode::kAlreadyExists;
    case absl::StatusCode::kPermissionDenied:
      return mojo_base::mojom::AbslStatusCode::kPermissionDenied;
    case absl::StatusCode::kResourceExhausted:
      return mojo_base::mojom::AbslStatusCode::kResourceExhausted;
    case absl::StatusCode::kFailedPrecondition:
      return mojo_base::mojom::AbslStatusCode::kFailedPrecondition;
    case absl::StatusCode::kAborted:
      return mojo_base::mojom::AbslStatusCode::kAborted;
    case absl::StatusCode::kOutOfRange:
      return mojo_base::mojom::AbslStatusCode::kOutOfRange;
    case absl::StatusCode::kUnimplemented:
      return mojo_base::mojom::AbslStatusCode::kUnimplemented;
    case absl::StatusCode::kInternal:
      return mojo_base::mojom::AbslStatusCode::kInternal;
    case absl::StatusCode::kUnavailable:
      return mojo_base::mojom::AbslStatusCode::kUnavailable;
    case absl::StatusCode::kDataLoss:
      return mojo_base::mojom::AbslStatusCode::kDataLoss;
    case absl::StatusCode::kUnauthenticated:
      return mojo_base::mojom::AbslStatusCode::kUnauthenticated;
    case absl::StatusCode::
        kDoNotUseReservedForFutureExpansionUseDefaultInSwitchInstead_:
      NOTREACHED_NORETURN();
  }
}

mojom::ActionType ConvertActionTypeToMojom(uint32_t action) {
  switch (::nearby::presence::ActionBit(action)) {
    case ::nearby::presence::ActionBit::kActiveUnlockAction:
      return mojom::ActionType::kActiveUnlockAction;
    case ::nearby::presence::ActionBit::kNearbyShareAction:
      return mojom::ActionType::kNearbyShareAction;
    case ::nearby::presence::ActionBit::kInstantTetheringAction:
      return mojom::ActionType::kInstantTetheringAction;
    case ::nearby::presence::ActionBit::kPhoneHubAction:
      return mojom::ActionType::kPhoneHubAction;
    case ::nearby::presence::ActionBit::kPresenceManagerAction:
      return mojom::ActionType::kPresenceManagerAction;
    case ::nearby::presence::ActionBit::kFinderAction:
      return mojom::ActionType::kFinderAction;
    case ::nearby::presence::ActionBit::kFastPairSassAction:
      return mojom::ActionType::kFastPairSassAction;
    case ::nearby::presence::ActionBit::kTapToTransferAction:
      return mojom::ActionType::kTapToTransferAction;
    case ::nearby::presence::ActionBit::kLastAction:
      return mojom::ActionType::kLastAction;
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
          shared_credential.metadata_encryption_key_tag_v0().begin(),
          shared_credential.metadata_encryption_key_tag_v0().end()),
      std::vector<uint8_t>(
          shared_credential.connection_signature_verification_key().begin(),
          shared_credential.connection_signature_verification_key().end()),
      std::vector<uint8_t>(
          shared_credential.advertisement_signature_verification_key().begin(),
          shared_credential.advertisement_signature_verification_key().end()),
      ConvertIdentityTypeToMojom(shared_credential.identity_type()),
      std::vector<uint8_t>(shared_credential.version().begin(),
                           shared_credential.version().end()));
}

::nearby::internal::SharedCredential SharedCredentialFromMojom(
    mojom::SharedCredential* shared_credential) {
  ::nearby::internal::SharedCredential proto;
  proto.set_secret_id(std::string(shared_credential->secret_id.begin(),
                                  shared_credential->secret_id.end()));
  proto.set_key_seed(std::string(shared_credential->key_seed.begin(),
                                 shared_credential->key_seed.end()));
  proto.set_start_time_millis(shared_credential->start_time_millis);
  proto.set_end_time_millis(shared_credential->end_time_millis);
  proto.set_encrypted_metadata_bytes_v0(
      std::string(shared_credential->encrypted_metadata_bytes.begin(),
                  shared_credential->encrypted_metadata_bytes.end()));
  proto.set_metadata_encryption_key_tag_v0(
      std::string(shared_credential->metadata_encryption_key_tag.begin(),
                  shared_credential->metadata_encryption_key_tag.end()));
  proto.set_connection_signature_verification_key(std::string(
      shared_credential->connection_signature_verification_key.begin(),
      shared_credential->connection_signature_verification_key.end()));
  proto.set_advertisement_signature_verification_key(std::string(
      shared_credential->advertisement_signature_verification_key.begin(),
      shared_credential->advertisement_signature_verification_key.end()));
  proto.set_identity_type(
      ConvertMojomIdentityType(shared_credential->identity_type));
  proto.set_version(std::string(shared_credential->version.begin(),
                                shared_credential->version.end()));
  return proto;
}

mojom::PresenceDevicePtr BuildPresenceMojomDevice(
    ::nearby::presence::PresenceDevice device) {
  std::vector<mojom::ActionType> actions;
  for (auto action : device.GetActions()) {
    actions.push_back(ConvertActionTypeToMojom(action.GetActionIdentifier()));
  }

  // TODO(b/276642472): Properly plumb type and stable_device_id.
  return mojom::PresenceDevice::New(device.GetEndpointId(), std::move(actions),
                                    /*stable_device_id=*/absl::nullopt,
                                    MetadataToMojom(device.GetMetadata()));
}

}  // namespace ash::nearby::presence
