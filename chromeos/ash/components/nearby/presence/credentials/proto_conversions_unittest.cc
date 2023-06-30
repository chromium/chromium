// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/proto_conversions.h"

#include "base/test/gtest_util.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kUserEmail = "testtester@gmail.com";
const std::string kDeviceName = "Test's Chromebook";
const std::string kUserName = "Test Tester";
const std::string kProfileUrl = "https://example.com";
const std::string kMacAddress = "1A:2B:3C:4D:5E:6F";
const std::vector<uint8_t> kSecretId = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
const std::vector<uint8_t> kKeySeed = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22};
const std::vector<uint8_t> kEncryptedMetadataBytes = {0x33, 0x33, 0x33,
                                                      0x33, 0x33, 0x33};
const std::vector<uint8_t> kMetadataEncryptionTag = {0x44, 0x44, 0x44,
                                                     0x44, 0x44, 0x44};
const std::vector<uint8_t> kConnectionSignatureVerificationKey = {
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
const std::vector<uint8_t> kAdvertisementSignatureVerificationKey = {
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66};
const std::vector<uint8_t> kVersion = {0x77, 0x77, 0x77, 0x77, 0x77, 0x77};

// The start and end time values are converted from milliseconds in the NP
// library to seconds to be stored in the NP server. When the credentials are
// downloaded, the start and end times are converted from seconds to
// milliseconds, and because these values are stored as ints, they are
// expected to lose preciseness.
constexpr int64_t kStartTimeMillis_BeforeConversion = 255486129307;
constexpr int64_t kEndTimeMillis_BeforeConversion = 64301728896;
constexpr int64_t kStartTimeMillis_AfterConversion = 255486129000;
constexpr int64_t kEndTimeMillis_AfterConversion = 64301728000;

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
      proto_cred.metadata_encryption_key_unsigned_adv_tag());
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
  shared_credential.set_metadata_encryption_key_unsigned_adv_tag(std::string(
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
      proto_cred.metadata_encryption_key_unsigned_adv_tag());
  EXPECT_EQ(::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE,
            proto_cred.identity_type());
}

}  // namespace ash::nearby::presence::proto
