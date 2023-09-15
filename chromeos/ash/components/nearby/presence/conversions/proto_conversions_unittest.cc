// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"

#include "base/test/gtest_util.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kUserEmail = "testtester@gmail.com";
const std::string kDeviceName = "Test's Chromebook";
const std::string kUserName = "Test Tester";
const std::string kProfileUrl = "https://example.com";
const std::string kMacAddress = "1A:2B:3C:4D:5E:6F";
const std::string AdvertisementSigningKeyCertificateAlias =
    "NearbySharingYWJjZGVmZ2hpamtsbW5vcA";
const std::string ConnectionSigningKeyCertificateAlias =
    "NearbySharingCJfjKGVmZ2hpJCtsbC5vDb";
const std::vector<uint8_t> kSecretId = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
const std::vector<uint8_t> kKeySeed = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22};
const std::vector<uint8_t> kEncryptedMetadataBytes = {0x33, 0x33, 0x33,
                                                      0x33, 0x33, 0x33};
const std::vector<uint8_t> kMetadataEncryptionTag = {0x44, 0x44, 0x44,
                                                     0x44, 0x44, 0x44};
const std::vector<uint8_t> kAdvertisementMetadataEncrpytionKeyV0 = {
    0x44, 0x45, 0x46, 0x47, 0x44, 0x44, 0x44,
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44};
const std::vector<uint8_t> kAdvertisementMetadataEncrpytionKeyV1 = {
    0x44, 0x45, 0x46, 0x47, 0x44, 0x44, 0x44, 0x44,
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44};
const std::vector<uint8_t> kPrivateKey = {0x44, 0x44, 0x46, 0x74, 0x44, 0x44};
const std::vector<uint8_t> kConnectionSignatureVerificationKey = {
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
const std::vector<uint8_t> kAdvertisementSignatureVerificationKey = {
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66};
const std::vector<uint8_t> kVersion = {0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
const std::map<uint32_t, bool> kConsumedSalts = {{0xb412, true},
                                                 {0x34b2, false},
                                                 {0x5171, false}};

// The start and end time values are converted from milliseconds in the NP
// library to seconds to be stored in the NP server. When the credentials are
// downloaded, the start and end times are converted from seconds to
// milliseconds, and because these values are stored as ints, they are
// expected to lose preciseness.
constexpr int64_t kStartTimeMillis_BeforeConversion = 255486129307;
constexpr int64_t kEndTimeMillis_BeforeConversion = 64301728896;
constexpr int64_t kStartTimeMillis_AfterConversion = 255486129000;
constexpr int64_t kEndTimeMillis_AfterConversion = 64301728000;

void SetProtoMap(::google::protobuf::Map<uint32_t, bool>* proto_map,
                 const std::map<uint32_t, bool>& map) {
  for (const auto& element : map) {
    proto_map->insert({element.first, element.second});
  }
}

::nearby::internal::LocalCredential::PrivateKey* CreatePrivateKeyProto(
    std::string certificate_alias,
    std::vector<uint8_t> key) {
  ::nearby::internal::LocalCredential::PrivateKey* private_key_proto =
      new ::nearby::internal::LocalCredential::PrivateKey;
  private_key_proto->set_certificate_alias(std::string(certificate_alias));
  private_key_proto->set_key(std::string(key.begin(), key.end()));
  return private_key_proto;
}

}  // namespace

namespace ash::nearby::presence::proto {

class ProtoConversionsTest : public testing::Test {};

TEST_F(ProtoConversionsTest, BuildMetadata) {
  ::nearby::internal::Metadata metadata = BuildMetadata(
      /*device_type=*/::nearby::internal::DeviceType::DEVICE_TYPE_LAPTOP,
      /*account_name=*/kUserEmail,
      /*device_name=*/kDeviceName,
      /*user_name=*/kUserName,
      /*profile_url=*/kProfileUrl,
      /*mac_address=*/kMacAddress);

  EXPECT_EQ(kUserEmail, metadata.account_name());
  EXPECT_EQ(kDeviceName, metadata.device_name());
  EXPECT_EQ(kUserName, metadata.user_name());
  EXPECT_EQ(kProfileUrl, metadata.device_profile_url());
  EXPECT_EQ(kMacAddress, metadata.bluetooth_mac_address());
}

TEST_F(ProtoConversionsTest, DeviceTypeToMojo) {
  EXPECT_EQ(
      mojom::PresenceDeviceType::kChromeos,
      DeviceTypeToMojom(::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS));
}

TEST_F(ProtoConversionsTest, PublicCredentialTypeToMojom) {
  EXPECT_EQ(
      mojom::PublicCredentialType::kLocalPublicCredential,
      PublicCredentialTypeToMojom(
          ::nearby::presence::PublicCredentialType::kLocalPublicCredential));

  EXPECT_EQ(
      mojom::PublicCredentialType::kRemotePublicCredential,
      PublicCredentialTypeToMojom(
          ::nearby::presence::PublicCredentialType::kRemotePublicCredential));
}

TEST_F(ProtoConversionsTest, MetadataToMojom) {
  ::nearby::internal::Metadata metadata = BuildMetadata(
      /*device_type=*/::nearby::internal::DeviceType::DEVICE_TYPE_LAPTOP,
      /*account_name=*/kUserEmail,
      /*device_name=*/kDeviceName,
      /*user_name=*/kUserName,
      /*profile_url=*/kProfileUrl,
      /*mac_address=*/kMacAddress);
  mojom::MetadataPtr mojo = MetadataToMojom(metadata);

  EXPECT_EQ(kUserEmail, mojo->account_name);
  EXPECT_EQ(kDeviceName, mojo->device_name);
  EXPECT_EQ(kUserName, mojo->user_name);
  EXPECT_EQ(kProfileUrl, mojo->device_profile_url);
  EXPECT_EQ(kMacAddress, std::string(mojo->bluetooth_mac_address.begin(),
                                     mojo->bluetooth_mac_address.end()));
}

TEST_F(ProtoConversionsTest, IdentityTypeFromMojom) {
  EXPECT_EQ(::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE,
            IdentityTypeFromMojom(mojom::IdentityType::kIdentityTypePrivate));
}

TEST_F(ProtoConversionsTest, SharedCredentialFromMojom) {
  mojom::SharedCredentialPtr mojo_cred = mojom::SharedCredential::New(
      kSecretId, kKeySeed, kStartTimeMillis_BeforeConversion,
      kEndTimeMillis_BeforeConversion, kEncryptedMetadataBytes,
      kMetadataEncryptionTag, kConnectionSignatureVerificationKey,
      kAdvertisementSignatureVerificationKey,
      mojom::IdentityType::kIdentityTypePrivate, kVersion);
  ::nearby::internal::SharedCredential proto_cred =
      SharedCredentialFromMojom(mojo_cred.get());
  EXPECT_EQ(std::string(kSecretId.begin(), kSecretId.end()),
            proto_cred.secret_id());
  EXPECT_EQ(std::string(kKeySeed.begin(), kKeySeed.end()),
            proto_cred.key_seed());
  EXPECT_EQ(kStartTimeMillis_BeforeConversion, proto_cred.start_time_millis());
  EXPECT_EQ(kEndTimeMillis_BeforeConversion, proto_cred.end_time_millis());
  EXPECT_EQ(std::string(kEncryptedMetadataBytes.begin(),
                        kEncryptedMetadataBytes.end()),
            proto_cred.encrypted_metadata_bytes_v0());
  EXPECT_EQ(
      std::string(kMetadataEncryptionTag.begin(), kMetadataEncryptionTag.end()),
      proto_cred.metadata_encryption_key_tag_v0());
  EXPECT_EQ(std::string(kConnectionSignatureVerificationKey.begin(),
                        kConnectionSignatureVerificationKey.end()),
            proto_cred.connection_signature_verification_key());
  EXPECT_EQ(std::string(kAdvertisementSignatureVerificationKey.begin(),
                        kAdvertisementSignatureVerificationKey.end()),
            proto_cred.advertisement_signature_verification_key());
  EXPECT_EQ(::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE,
            proto_cred.identity_type());
  EXPECT_EQ(std::string(kVersion.begin(), kVersion.end()),
            proto_cred.version());
}

TEST_F(ProtoConversionsTest, PublicCertificateFromSharedCredential) {
  ::nearby::internal::SharedCredential shared_credential;
  shared_credential.set_secret_id(
      std::string(kSecretId.begin(), kSecretId.end()));
  shared_credential.set_key_seed(std::string(kKeySeed.begin(), kKeySeed.end()));
  shared_credential.set_start_time_millis(kStartTimeMillis_BeforeConversion);
  shared_credential.set_end_time_millis(kEndTimeMillis_BeforeConversion);
  shared_credential.set_encrypted_metadata_bytes_v0(std::string(
      kEncryptedMetadataBytes.begin(), kEncryptedMetadataBytes.end()));
  shared_credential.set_metadata_encryption_key_tag_v0(std::string(
      kMetadataEncryptionTag.begin(), kMetadataEncryptionTag.end()));
  shared_credential.set_connection_signature_verification_key(
      std::string(kConnectionSignatureVerificationKey.begin(),
                  kConnectionSignatureVerificationKey.end()));
  shared_credential.set_advertisement_signature_verification_key(
      std::string(kAdvertisementSignatureVerificationKey.begin(),
                  kAdvertisementSignatureVerificationKey.end()));
  shared_credential.set_identity_type(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE);

  ash::nearby::proto::PublicCertificate proto_cert =
      PublicCertificateFromSharedCredential(shared_credential);
  EXPECT_EQ(std::string(kSecretId.begin(), kSecretId.end()),
            proto_cert.secret_id());
  EXPECT_EQ(std::string(kKeySeed.begin(), kKeySeed.end()),
            proto_cert.secret_key());
  EXPECT_EQ(std::string(kConnectionSignatureVerificationKey.begin(),
                        kConnectionSignatureVerificationKey.end()),
            proto_cert.public_key());
  EXPECT_EQ(MillisecondsToSeconds(kStartTimeMillis_BeforeConversion),
            proto_cert.start_time().seconds());
  EXPECT_EQ(MillisecondsToSeconds(kEndTimeMillis_BeforeConversion),
            proto_cert.end_time().seconds());
  EXPECT_EQ(std::string(kEncryptedMetadataBytes.begin(),
                        kEncryptedMetadataBytes.end()),
            proto_cert.encrypted_metadata_bytes());
  EXPECT_EQ(
      std::string(kMetadataEncryptionTag.begin(), kMetadataEncryptionTag.end()),
      proto_cert.metadata_encryption_key_tag());
  EXPECT_EQ(ash::nearby::proto::TrustType::TRUST_TYPE_PRIVATE,
            proto_cert.trust_type());
}

TEST_F(ProtoConversionsTest, PublicCertificateToSharedCredential) {
  ash::nearby::proto::PublicCertificate certificate;
  certificate.set_secret_id(std::string(kSecretId.begin(), kSecretId.end()));
  certificate.set_secret_key(std::string(kKeySeed.begin(), kKeySeed.end()));
  certificate.set_public_key(
      std::string(kConnectionSignatureVerificationKey.begin(),
                  kConnectionSignatureVerificationKey.end()));
  certificate.mutable_start_time()->set_seconds(
      MillisecondsToSeconds(kStartTimeMillis_BeforeConversion));
  certificate.mutable_end_time()->set_seconds(
      MillisecondsToSeconds(kEndTimeMillis_BeforeConversion));
  certificate.set_encrypted_metadata_bytes(std::string(
      kEncryptedMetadataBytes.begin(), kEncryptedMetadataBytes.end()));
  certificate.set_metadata_encryption_key_tag(std::string(
      kMetadataEncryptionTag.begin(), kMetadataEncryptionTag.end()));
  certificate.set_trust_type(ash::nearby::proto::TrustType::TRUST_TYPE_PRIVATE);

  ::nearby::internal::SharedCredential proto_cred =
      PublicCertificateToSharedCredential(certificate);
  EXPECT_EQ(std::string(kSecretId.begin(), kSecretId.end()),
            proto_cred.secret_id());
  EXPECT_EQ(std::string(kKeySeed.begin(), kKeySeed.end()),
            proto_cred.key_seed());
  EXPECT_EQ(std::string(kConnectionSignatureVerificationKey.begin(),
                        kConnectionSignatureVerificationKey.end()),
            proto_cred.connection_signature_verification_key());
  EXPECT_EQ(kStartTimeMillis_AfterConversion, proto_cred.start_time_millis());
  EXPECT_EQ(kEndTimeMillis_AfterConversion, proto_cred.end_time_millis());
  EXPECT_EQ(std::string(kEncryptedMetadataBytes.begin(),
                        kEncryptedMetadataBytes.end()),
            proto_cred.encrypted_metadata_bytes_v0());
  EXPECT_EQ(
      std::string(kMetadataEncryptionTag.begin(), kMetadataEncryptionTag.end()),
      proto_cred.metadata_encryption_key_tag_v0());
  EXPECT_EQ(::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE,
            proto_cred.identity_type());
}

TEST_F(ProtoConversionsTest, PrivateKeyToMojomTest) {
  ::nearby::internal::LocalCredential::PrivateKey private_key;
  private_key.set_certificate_alias(
      std::string(AdvertisementSigningKeyCertificateAlias.begin(),
                  AdvertisementSigningKeyCertificateAlias.end()));
  private_key.set_key(std::string(kPrivateKey.begin(), kPrivateKey.end()));

  mojom::PrivateKeyPtr mojo_private_key = PrivateKeyToMojom(private_key);

  EXPECT_EQ(AdvertisementSigningKeyCertificateAlias,
            mojo_private_key->certificate_alias);
  EXPECT_EQ(kPrivateKey, mojo_private_key->key);
}

TEST_F(ProtoConversionsTest, PrivateKeyFromMojomTest) {
  mojom::PrivateKeyPtr mojo_private_key = mojom::PrivateKey::New(
      AdvertisementSigningKeyCertificateAlias, kPrivateKey);

  ::nearby::internal::LocalCredential::PrivateKey proto_private_key =
      PrivateKeyFromMojom(mojo_private_key.get());

  EXPECT_EQ(std::string(AdvertisementSigningKeyCertificateAlias.begin(),
                        AdvertisementSigningKeyCertificateAlias.end()),
            proto_private_key.certificate_alias());
  EXPECT_EQ(std::string(kPrivateKey.begin(), kPrivateKey.end()),
            proto_private_key.key());
}

TEST_F(ProtoConversionsTest, LocalCredentialToMojomTest) {
  ::nearby::internal::LocalCredential local_credential;
  local_credential.set_secret_id(
      std::string(kSecretId.begin(), kSecretId.end()));
  local_credential.set_key_seed(std::string(kKeySeed.begin(), kKeySeed.end()));
  local_credential.set_start_time_millis(kStartTimeMillis_BeforeConversion);
  local_credential.set_metadata_encryption_key_v0(
      std::string(kAdvertisementMetadataEncrpytionKeyV0.begin(),
                  kAdvertisementMetadataEncrpytionKeyV0.end()));
  local_credential.set_allocated_advertisement_signing_key(
      CreatePrivateKeyProto(AdvertisementSigningKeyCertificateAlias,
                            kPrivateKey));
  local_credential.set_allocated_connection_signing_key(
      CreatePrivateKeyProto(ConnectionSigningKeyCertificateAlias, kPrivateKey));
  local_credential.set_identity_type(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE);
  SetProtoMap(local_credential.mutable_consumed_salts(), kConsumedSalts);
  local_credential.set_metadata_encryption_key_v1(
      std::string(kAdvertisementMetadataEncrpytionKeyV1.begin(),
                  kAdvertisementMetadataEncrpytionKeyV1.end()));

  mojom::LocalCredentialPtr mojo_local_credential =
      LocalCredentialToMojom(local_credential);

  EXPECT_EQ(kSecretId, mojo_local_credential->secret_id);
  EXPECT_EQ(kKeySeed, mojo_local_credential->key_seed);
  EXPECT_EQ(kStartTimeMillis_BeforeConversion,
            mojo_local_credential->start_time_millis);
  EXPECT_EQ(kAdvertisementMetadataEncrpytionKeyV0,
            mojo_local_credential->metadata_encryption_key_v0);
  EXPECT_EQ(mojom::IdentityType::kIdentityTypePrivate,
            mojo_local_credential->identity_type);
  EXPECT_EQ(kConsumedSalts.size(),
            mojo_local_credential->consumed_salts.size());
  EXPECT_EQ(kAdvertisementMetadataEncrpytionKeyV1,
            mojo_local_credential->metadata_encryption_key_v1);
  // advertisement_signing_key
  EXPECT_EQ(
      AdvertisementSigningKeyCertificateAlias,
      mojo_local_credential->advertisement_signing_key->certificate_alias);
  EXPECT_EQ(kPrivateKey, mojo_local_credential->advertisement_signing_key->key);
  // connection_singing_key
  EXPECT_EQ(ConnectionSigningKeyCertificateAlias,
            mojo_local_credential->connection_signing_key->certificate_alias);
  EXPECT_EQ(kPrivateKey, mojo_local_credential->connection_signing_key->key);
}

TEST_F(ProtoConversionsTest, LocalCredentialFromMojomTest) {
  base::flat_map<uint32_t, bool> kConsumedSalts_flat(kConsumedSalts.begin(),
                                                     kConsumedSalts.end());
  mojom::LocalCredentialPtr mojo_local_credential = mojom::LocalCredential::New(
      kSecretId, kKeySeed, kStartTimeMillis_BeforeConversion,
      kAdvertisementMetadataEncrpytionKeyV0,
      mojom::PrivateKey::New(AdvertisementSigningKeyCertificateAlias,
                             kPrivateKey),
      mojom::PrivateKey::New(ConnectionSigningKeyCertificateAlias, kPrivateKey),
      mojom::IdentityType::kIdentityTypePrivate, kConsumedSalts_flat,
      kAdvertisementMetadataEncrpytionKeyV1);

  ::nearby::internal::LocalCredential local_credential_proto =
      LocalCredentialFromMojom(mojo_local_credential.get());

  EXPECT_EQ(std::string(kSecretId.begin(), kSecretId.end()),
            local_credential_proto.secret_id());
  EXPECT_EQ(std::string(kKeySeed.begin(), kKeySeed.end()),
            local_credential_proto.key_seed());
  EXPECT_EQ(kStartTimeMillis_BeforeConversion,
            local_credential_proto.start_time_millis());
  EXPECT_EQ(std::string(kAdvertisementMetadataEncrpytionKeyV0.begin(),
                        kAdvertisementMetadataEncrpytionKeyV0.end()),
            local_credential_proto.metadata_encryption_key_v0());
  EXPECT_EQ(::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE,
            local_credential_proto.identity_type());
  EXPECT_EQ(kConsumedSalts.size(),
            local_credential_proto.consumed_salts().size());
  EXPECT_EQ(std::string(kAdvertisementMetadataEncrpytionKeyV1.begin(),
                        kAdvertisementMetadataEncrpytionKeyV1.end()),
            local_credential_proto.metadata_encryption_key_v1());
  // advertisement_signing_key
  EXPECT_EQ(
      AdvertisementSigningKeyCertificateAlias,
      local_credential_proto.advertisement_signing_key().certificate_alias());
  EXPECT_EQ(std::string(kPrivateKey.begin(), kPrivateKey.end()),
            local_credential_proto.advertisement_signing_key().key());
  // connection_signing_key
  EXPECT_EQ(
      ConnectionSigningKeyCertificateAlias,
      local_credential_proto.connection_signing_key().certificate_alias());
  EXPECT_EQ(std::string(kPrivateKey.begin(), kPrivateKey.end()),
            local_credential_proto.connection_signing_key().key());
}

}  // namespace ash::nearby::presence::proto
