// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"

namespace ash::nearby::presence::proto {

namespace {

::nearby::internal::CredentialType
RemoteCredentialTypeToThirdPartyCredentialType(
    ash::nearby::proto::CredentialType remote_credential_type) {
  switch (remote_credential_type) {
    case ash::nearby::proto::CredentialType::CREDENTIAL_TYPE_UNKNOWN:
      return ::nearby::internal::CredentialType::CREDENTIAL_TYPE_UNKNOWN;
    case ash::nearby::proto::CredentialType::CREDENTIAL_TYPE_DEVICE:
      return ::nearby::internal::CredentialType::CREDENTIAL_TYPE_DEVICE;
    case ash::nearby::proto::CredentialType::CREDENTIAL_TYPE_GAIA:
      return ::nearby::internal::CredentialType::CREDENTIAL_TYPE_GAIA;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED();
}

::nearby::internal::IdentityType RemoteIdentityTypeToThirdPartyIdentityType(
    ash::nearby::proto::IdentityType remote_identity_type) {
  switch (remote_identity_type) {
    case ash::nearby::proto::IdentityType::IDENTITY_TYPE_UNSPECIFIED:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
    case ash::nearby::proto::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP;
    case ash::nearby::proto::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  NOTREACHED();
}

}  // namespace

::nearby::internal::DeviceIdentityMetaData BuildMetadata(
    ::nearby::internal::DeviceType device_type,
    const std::string& device_name,
    const std::string& mac_address,
    const std::string& device_id) {
  ::nearby::internal::DeviceIdentityMetaData proto;
  proto.set_device_type(device_type);
  proto.set_device_name(device_name);
  proto.set_bluetooth_mac_address(mac_address);
  proto.set_device_id(device_id);
  return proto;
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

// TODO(b:300510287): Use EnumTrait<> to convert between enums rather than
// a method call.
mojom::PublicCredentialType PublicCredentialTypeToMojom(
    ::nearby::presence::PublicCredentialType public_credential_type) {
  switch (public_credential_type) {
    case ::nearby::presence::PublicCredentialType::kLocalPublicCredential:
      return mojom::PublicCredentialType::kLocalPublicCredential;
    case ::nearby::presence::PublicCredentialType::kRemotePublicCredential:
      return mojom::PublicCredentialType::kRemotePublicCredential;
  }

  NOTREACHED();
}

mojom::PrivateKeyPtr PrivateKeyToMojom(
    ::nearby::internal::LocalCredential::PrivateKey private_key) {
  return mojom::PrivateKey::New(
      private_key.certificate_alias(),
      std::vector<uint8_t>(private_key.key().begin(), private_key.key().end()));
}

::nearby::internal::IdentityType IdentityTypeFromMojom(
    mojom::IdentityType identity_type) {
  switch (identity_type) {
    case mojom::IdentityType::kIdentityTypeUnspecified:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
    case mojom::IdentityType::kIdentityTypePrivateGroup:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP;
    case mojom::IdentityType::kIdentityTypeContactsGroup:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP;
    case mojom::IdentityType::kIdentityTypePublic:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC;
    default:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
  }
}

mojom::MetadataPtr MetadataToMojom(
    ::nearby::internal::DeviceIdentityMetaData metadata) {
  return mojom::Metadata::New(
      DeviceTypeToMojom(metadata.device_type()), metadata.device_name(),
      std::vector<uint8_t>(metadata.bluetooth_mac_address().begin(),
                           metadata.bluetooth_mac_address().end()),
      std::vector<uint8_t>(metadata.device_id().begin(),
                           metadata.device_id().end()));
}

::nearby::internal::LocalCredential::PrivateKey PrivateKeyFromMojom(
    mojom::PrivateKey* private_key) {
  ::nearby::internal::LocalCredential::PrivateKey proto;
  proto.set_certificate_alias(
      std::string(private_key->certificate_alias.begin(),
                  private_key->certificate_alias.end()));
  proto.set_key(std::string(private_key->key.begin(), private_key->key.end()));
  return proto;
}

::nearby::internal::CredentialType CredentialTypeFromMojom(
    mojom::CredentialType credential_type) {
  switch (credential_type) {
    case mojom::CredentialType::kCredentialTypeUnknown:
      return ::nearby::internal::CredentialType::CREDENTIAL_TYPE_UNKNOWN;
    case mojom::CredentialType::kCredentialTypeDevice:
      return ::nearby::internal::CredentialType::CREDENTIAL_TYPE_DEVICE;
    case mojom::CredentialType::kCredentialTypeGaia:
      return ::nearby::internal::CredentialType::CREDENTIAL_TYPE_GAIA;
    default:
      return ::nearby::internal::CredentialType::CREDENTIAL_TYPE_UNKNOWN;
  }
}

::nearby::internal::SharedCredential SharedCredentialFromMojom(
    mojom::SharedCredential* shared_credential) {
  ::nearby::internal::SharedCredential proto;
  proto.set_key_seed(std::string(shared_credential->key_seed.begin(),
                                 shared_credential->key_seed.end()));
  proto.set_start_time_millis(shared_credential->start_time_millis);
  proto.set_end_time_millis(shared_credential->end_time_millis);
  proto.set_encrypted_metadata_bytes_v0(
      std::string(shared_credential->encrypted_metadata_bytes_v0.begin(),
                  shared_credential->encrypted_metadata_bytes_v0.end()));
  proto.set_metadata_encryption_key_tag_v0(
      std::string(shared_credential->metadata_encryption_key_tag_v0.begin(),
                  shared_credential->metadata_encryption_key_tag_v0.end()));
  proto.set_connection_signature_verification_key(std::string(
      shared_credential->connection_signature_verification_key.begin(),
      shared_credential->connection_signature_verification_key.end()));
  proto.set_advertisement_signature_verification_key(std::string(
      shared_credential->advertisement_signature_verification_key.begin(),
      shared_credential->advertisement_signature_verification_key.end()));
  proto.set_identity_type(
      IdentityTypeFromMojom(shared_credential->identity_type));
  proto.set_version(std::string(shared_credential->version.begin(),
                                shared_credential->version.end()));
  proto.set_credential_type(
      CredentialTypeFromMojom(shared_credential->credential_type));
  proto.set_encrypted_metadata_bytes_v1(
      std::string(shared_credential->encrypted_metadata_bytes_v1.begin(),
                  shared_credential->encrypted_metadata_bytes_v1.end()));
  proto.set_identity_token_short_salt_adv_hmac_key_v1(std::string(
      shared_credential->identity_token_short_salt_adv_hmac_key_v1.begin(),
      shared_credential->identity_token_short_salt_adv_hmac_key_v1.end()));
  proto.set_id(shared_credential->id);
  proto.set_dusi(shared_credential->dusi);
  proto.set_signature_version(shared_credential->signature_version);
  proto.set_identity_token_extended_salt_adv_hmac_key_v1(std::string(
      shared_credential->identity_token_extended_salt_adv_hmac_key_v1.begin(),
      shared_credential->identity_token_extended_salt_adv_hmac_key_v1.end()));
  proto.set_identity_token_signed_adv_hmac_key_v1(std::string(
      shared_credential->identity_token_signed_adv_hmac_key_v1.begin(),
      shared_credential->identity_token_signed_adv_hmac_key_v1.end()));
  return proto;
}

::nearby::internal::LocalCredential LocalCredentialFromMojom(
    mojom::LocalCredential* local_credential) {
  ::nearby::internal::LocalCredential proto;
  proto.set_secret_id(std::string(local_credential->secret_id.begin(),
                                  local_credential->secret_id.end()));
  proto.set_key_seed(std::string(local_credential->key_seed.begin(),
                                 local_credential->key_seed.end()));
  proto.set_start_time_millis(local_credential->start_time_millis);
  proto.set_end_time_millis(local_credential->end_time_millis);
  proto.set_metadata_encryption_key_v0(
      std::string(local_credential->metadata_encryption_key_v0.begin(),
                  local_credential->metadata_encryption_key_v0.end()));

  auto* advertisement_signing_key =
      new ::nearby::internal::LocalCredential::PrivateKey(PrivateKeyFromMojom(
          local_credential->advertisement_signing_key.get()));
  proto.set_allocated_advertisement_signing_key(advertisement_signing_key);

  auto* connection_signing_key =
      new ::nearby::internal::LocalCredential::PrivateKey(
          PrivateKeyFromMojom(local_credential->connection_signing_key.get()));
  proto.set_allocated_connection_signing_key(connection_signing_key);

  proto.set_identity_type(
      IdentityTypeFromMojom(local_credential->identity_type));

  for (const auto& pair : local_credential->consumed_salts) {
    auto map_pair =
        ::google::protobuf::MapPair<uint32_t, bool>(pair.first, pair.second);
    proto.mutable_consumed_salts()->insert(map_pair);
  }

  proto.set_identity_token_v1(
      std::string(local_credential->identity_token_v1.begin(),
                  local_credential->identity_token_v1.end()));
  proto.set_id(local_credential->id);
  proto.set_signature_version(local_credential->signature_version);

  return proto;
}

mojom::IdentityType IdentityTypeToMojom(
    ::nearby::internal::IdentityType identity_type) {
  switch (identity_type) {
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED:
      return mojom::IdentityType::kIdentityTypeUnspecified;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP:
      return mojom::IdentityType::kIdentityTypePrivateGroup;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP:
      return mojom::IdentityType::kIdentityTypeContactsGroup;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC:
      return mojom::IdentityType::kIdentityTypePublic;
    default:
      return mojom::IdentityType::kIdentityTypeUnspecified;
  }
}

mojom::CredentialType CredentialTypeToMojom(
    ::nearby::internal::CredentialType credential_type) {
  switch (credential_type) {
    case ::nearby::internal::CredentialType::CREDENTIAL_TYPE_UNKNOWN:
      return mojom::CredentialType::kCredentialTypeUnknown;
    case ::nearby::internal::CredentialType::CREDENTIAL_TYPE_DEVICE:
      return mojom::CredentialType::kCredentialTypeDevice;
    case ::nearby::internal::CredentialType::CREDENTIAL_TYPE_GAIA:
      return mojom::CredentialType::kCredentialTypeGaia;
    default:
      return mojom::CredentialType::kCredentialTypeUnknown;
  }
}

mojom::SharedCredentialPtr SharedCredentialToMojom(
    ::nearby::internal::SharedCredential shared_credential) {
  return mojom::SharedCredential::New(
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
      IdentityTypeToMojom(shared_credential.identity_type()),
      std::vector<uint8_t>(shared_credential.version().begin(),
                           shared_credential.version().end()),
      CredentialTypeToMojom(shared_credential.credential_type()),
      std::vector<uint8_t>(
          shared_credential.encrypted_metadata_bytes_v1().begin(),
          shared_credential.encrypted_metadata_bytes_v1().end()),
      std::vector<uint8_t>(
          shared_credential.identity_token_short_salt_adv_hmac_key_v1().begin(),
          shared_credential.identity_token_short_salt_adv_hmac_key_v1().end()),
      shared_credential.id(), shared_credential.dusi(),
      shared_credential.signature_version(),
      std::vector<uint8_t>(
          shared_credential.identity_token_extended_salt_adv_hmac_key_v1()
              .begin(),
          shared_credential.identity_token_extended_salt_adv_hmac_key_v1()
              .end()),
      std::vector<uint8_t>(
          shared_credential.identity_token_signed_adv_hmac_key_v1().begin(),
          shared_credential.identity_token_signed_adv_hmac_key_v1().end()));
}

mojom::LocalCredentialPtr LocalCredentialToMojom(
    ::nearby::internal::LocalCredential local_credential) {
  base::flat_map<uint32_t, bool> salt_flat_map(
      local_credential.consumed_salts().begin(),
      local_credential.consumed_salts().end());

  return mojom::LocalCredential::New(
      std::vector<uint8_t>(local_credential.secret_id().begin(),
                           local_credential.secret_id().end()),
      std::vector<uint8_t>(local_credential.key_seed().begin(),
                           local_credential.key_seed().end()),
      local_credential.start_time_millis(), local_credential.end_time_millis(),
      std::vector<uint8_t>(
          local_credential.metadata_encryption_key_v0().begin(),
          local_credential.metadata_encryption_key_v0().end()),
      PrivateKeyToMojom(local_credential.advertisement_signing_key()),
      PrivateKeyToMojom(local_credential.connection_signing_key()),
      IdentityTypeToMojom(local_credential.identity_type()), salt_flat_map,
      std::vector<uint8_t>(local_credential.identity_token_v1().begin(),
                           local_credential.identity_token_v1().end()),
      local_credential.id(), local_credential.signature_version());
}

ash::nearby::proto::PublicCertificate PublicCertificateFromSharedCredential(
    ::nearby::internal::SharedCredential shared_credential) {
  ash::nearby::proto::PublicCertificate certificate;
  certificate.set_secret_id(shared_credential.secret_id());
  certificate.set_secret_key(shared_credential.key_seed());
  certificate.set_public_key(
      shared_credential.connection_signature_verification_key());
  certificate.mutable_start_time()->set_seconds(
      MillisecondsToSeconds(shared_credential.start_time_millis()));
  certificate.mutable_end_time()->set_seconds(
      MillisecondsToSeconds(shared_credential.end_time_millis()));
  certificate.set_encrypted_metadata_bytes(
      shared_credential.encrypted_metadata_bytes_v0());
  certificate.set_metadata_encryption_key_tag(
      shared_credential.metadata_encryption_key_tag_v0());
  certificate.set_trust_type(
      TrustTypeFromIdentityType(shared_credential.identity_type()));
  return certificate;
}

ash::nearby::proto::TrustType TrustTypeFromIdentityType(
    ::nearby::internal::IdentityType identity_type) {
  switch (identity_type) {
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED:
      return ash::nearby::proto::TrustType::TRUST_TYPE_UNSPECIFIED;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP:
      return ash::nearby::proto::TrustType::TRUST_TYPE_PRIVATE;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP:
      return ash::nearby::proto::TrustType::TRUST_TYPE_TRUSTED;
    default:
      return ash::nearby::proto::TrustType::TRUST_TYPE_UNSPECIFIED;
  }
}

int64_t MillisecondsToSeconds(int64_t milliseconds) {
  return milliseconds / 1000.0;
}

::nearby::internal::IdentityType TrustTypeToIdentityType(
    ash::nearby::proto::TrustType trust_type) {
  switch (trust_type) {
    case ash::nearby::proto::TrustType::TRUST_TYPE_UNSPECIFIED:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
    case ash::nearby::proto::TrustType::TRUST_TYPE_PRIVATE:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP;
    case ash::nearby::proto::TrustType::TRUST_TYPE_TRUSTED:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP;
    default:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
  }
}

::nearby::internal::SharedCredential
RemoteSharedCredentialToThirdPartySharedCredential(
    ash::nearby::proto::SharedCredential remote_shared_credential) {
  ::nearby::internal::SharedCredential shared_credential;
  shared_credential.set_id(remote_shared_credential.id());
  shared_credential.set_key_seed(remote_shared_credential.key_seed());
  shared_credential.set_start_time_millis(
      remote_shared_credential.start_time_millis());
  shared_credential.set_end_time_millis(
      remote_shared_credential.end_time_millis());

  shared_credential.set_encrypted_metadata_bytes_v0(
      remote_shared_credential.encrypted_metadata_bytes_v0());
  shared_credential.set_metadata_encryption_key_tag_v0(
      remote_shared_credential.metadata_encryption_key_tag_v0());
  shared_credential.set_connection_signature_verification_key(
      remote_shared_credential.connection_signature_verification_key());
  shared_credential.set_advertisement_signature_verification_key(
      remote_shared_credential.advertisement_signature_verification_key());

  shared_credential.set_identity_type(
      RemoteIdentityTypeToThirdPartyIdentityType(
          remote_shared_credential.identity_type()));
  shared_credential.set_version(remote_shared_credential.version());
  shared_credential.set_credential_type(
      RemoteCredentialTypeToThirdPartyCredentialType(
          remote_shared_credential.credential_type()));
  shared_credential.set_encrypted_metadata_bytes_v1(
      remote_shared_credential.encrypted_metadata_bytes_v1());
  shared_credential.set_identity_token_short_salt_adv_hmac_key_v1(
      remote_shared_credential.identity_token_short_salt_adv_hmac_key_v1());

  shared_credential.set_dusi(remote_shared_credential.dusi());
  shared_credential.set_signature_version(
      remote_shared_credential.signature_version());

  shared_credential.set_identity_token_extended_salt_adv_hmac_key_v1(
      remote_shared_credential.identity_token_extended_salt_adv_hmac_key_v1());
  shared_credential.set_identity_token_signed_adv_hmac_key_v1(
      remote_shared_credential.identity_token_signed_adv_hmac_key_v1());

  return shared_credential;
}

}  // namespace ash::nearby::presence::proto
