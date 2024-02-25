// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_key_proof_computer_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_proof_computer.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/hmac.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {
// Sample P-256 public and private keys from RFC 6979 A.2.5 in the respective
// ASN.1 formats: SubjectPublicKeyInfo (RFC 5280) and PKCS #8 PrivateKeyInfo
// (RFC 5208). Note: SecureMessage returns private keys (but not public keys) in
// this formatting.
const std::vector<uint8_t> kTestPublicKeyBytes = {
    0x30, 0x59,
    // Begin AlgorithmIdentifier: ecPublicKey, prime256v1
    0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06,
    0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
    // End AlgorithmIdentifier
    0x03, 0x42, 0x00, 0x04,
    // Public key bytes (Ux):
    0x60, 0xfe, 0xd4, 0xba, 0x25, 0x5a, 0x9d, 0x31, 0xc9, 0x61, 0xeb, 0x74,
    0xc6, 0x35, 0x6d, 0x68, 0xc0, 0x49, 0xb8, 0x92, 0x3b, 0x61, 0xfa, 0x6c,
    0xe6, 0x69, 0x62, 0x2e, 0x60, 0xf2, 0x9f, 0xb6,
    // Public key bytes (Uy):
    0x79, 0x03, 0xfe, 0x10, 0x08, 0xb8, 0xbc, 0x99, 0xa4, 0x1a, 0xe9, 0xe9,
    0x56, 0x28, 0xbc, 0x64, 0xf2, 0xf1, 0xb2, 0x0c, 0x2d, 0x7e, 0x9f, 0x51,
    0x77, 0xa3, 0xc2, 0x94, 0xd4, 0x46, 0x22, 0x99};
const std::vector<uint8_t> kTestPrivateKeyBytes = {
    0x30, 0x81, 0x87, 0x02, 0x01, 0x00,
    // Begin AlgorithmIdentifier: ecPublicKey, prime256v1
    0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06,
    0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
    // End AlgorithmIdentifier
    0x04, 0x6d, 0x30, 0x6b, 0x02, 0x01, 0x01, 0x04, 0x20,
    // Begin private key bytes
    0xc9, 0xaf, 0xa9, 0xd8, 0x45, 0xba, 0x75, 0x16, 0x6b, 0x5c, 0x21, 0x57,
    0x67, 0xb1, 0xd6, 0x93, 0x4e, 0x50, 0xc3, 0xdb, 0x36, 0xe8, 0x9b, 0x12,
    0x7b, 0x8a, 0x62, 0x2b, 0x12, 0x0f, 0x67, 0x21,
    // End private key bytes
    0xa1, 0x44, 0x03, 0x42, 0x00, 0x04,
    // Public key:
    0x60, 0xfe, 0xd4, 0xba, 0x25, 0x5a, 0x9d, 0x31, 0xc9, 0x61, 0xeb, 0x74,
    0xc6, 0x35, 0x6d, 0x68, 0xc0, 0x49, 0xb8, 0x92, 0x3b, 0x61, 0xfa, 0x6c,
    0xe6, 0x69, 0x62, 0x2e, 0x60, 0xf2, 0x9f, 0xb6, 0x79, 0x03, 0xfe, 0x10,
    0x08, 0xb8, 0xbc, 0x99, 0xa4, 0x1a, 0xe9, 0xe9, 0x56, 0x28, 0xbc, 0x64,
    0xf2, 0xf1, 0xb2, 0x0c, 0x2d, 0x7e, 0x9f, 0x51, 0x77, 0xa3, 0xc2, 0x94,
    0xd4, 0x46, 0x22, 0x99};
const std::string kAsymmetricTestSalt = "salt";

// For generating symmetric key proofs, we internally derive a key before
// signing. Here, we use the first HKDF test case from RFC 5869 so we have a
// known derived key to verify the HMAC signature with.
// Input key material (IKM):
const std::vector<uint8_t> kTestSymmetricKeyBytes(22, 0x0b);
// salt:
const std::vector<uint8_t> kSymmetricTestSaltBytes = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};
// info:
const std::vector<uint8_t> kSymmetricTestInfoBytes = {
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9};
// The first 16 bytes of output key material (OKM):
const std::vector<uint8_t> kExpectedDerivedSymmetricKey16Bytes = {
    0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a,
    0x90, 0x43, 0x4f, 0x64, 0xd0, 0x36, 0x2f, 0x2a};
// The first 32 bytes of output key material (OKM):
const std::vector<uint8_t> kExpectedDerivedSymmetricKey32Bytes = {
    0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90, 0x43, 0x4f,
    0x64, 0xd0, 0x36, 0x2f, 0x2a, 0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a,
    0x5a, 0x4c, 0x5d, 0xb0, 0x2d, 0x56, 0xec, 0xc4, 0xc5, 0xbf};

const std::string kTestPayload = "sample";

std::string ByteVectorToString(const std::vector<uint8_t>& byte_array) {
  return std::string(byte_array.begin(), byte_array.end());
}

std::vector<uint8_t> StringToByteVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

TEST(DeviceSyncCryptAuthKeyProofComputerImplTest,
     AsymmetricKeyProofComputation_Success) {
  CryptAuthKey key(ByteVectorToString(kTestPublicKeyBytes),
                   ByteVectorToString(kTestPrivateKeyBytes),
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256);

  std::optional<std::string> key_proof =
      CryptAuthKeyProofComputerImpl::Factory::Create()->ComputeKeyProof(
          key, kTestPayload, kAsymmetricTestSalt, std::nullopt /* info */);
  EXPECT_TRUE(key_proof);

  // Verify the key proof which should be of the form:
  //     Sign(|private_key|, |salt| + |payload|)
  //
  // Note: The signature is random, i.e., we have no way of setting "k" from RFC
  // 6979 A.2.5. So, we can only verify the signature using the public key.
  crypto::SignatureVerifier verifier;
  EXPECT_TRUE(verifier.VerifyInit(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      StringToByteVector(*key_proof), kTestPublicKeyBytes));
  verifier.VerifyUpdate(StringToByteVector(kAsymmetricTestSalt + kTestPayload));
  EXPECT_TRUE(verifier.VerifyFinal());
}

TEST(DeviceSyncCryptAuthKeyProofComputerImplTest,
     Symmetric256KeyProofComputation_Success) {
  CryptAuthKey key(ByteVectorToString(kTestSymmetricKeyBytes),
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::RAW256);

  std::optional<std::string> key_proof =
      CryptAuthKeyProofComputerImpl::Factory::Create()->ComputeKeyProof(
          key, kTestPayload, ByteVectorToString(kSymmetricTestSaltBytes),
          ByteVectorToString(kSymmetricTestInfoBytes));

  EXPECT_TRUE(key_proof);

  // Verify the key proof which should be of the form:
  //     HMAC(HKDF(|key|, |salt|, |info|), |payload|)
  crypto::HMAC hmac(crypto::HMAC::HashAlgorithm::SHA256);
  EXPECT_TRUE(
      hmac.Init(ByteVectorToString(kExpectedDerivedSymmetricKey32Bytes)));
  EXPECT_TRUE(hmac.Verify(kTestPayload, *key_proof));
}

TEST(DeviceSyncCryptAuthKeyProofComputerImplTest,
     Symmetric128KeyProofComputation_Success) {
  CryptAuthKey key(ByteVectorToString(kTestSymmetricKeyBytes),
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::RAW128);

  std::optional<std::string> key_proof =
      CryptAuthKeyProofComputerImpl::Factory::Create()->ComputeKeyProof(
          key, kTestPayload, ByteVectorToString(kSymmetricTestSaltBytes),
          ByteVectorToString(kSymmetricTestInfoBytes));
  EXPECT_TRUE(key_proof);

  crypto::HMAC hmac(crypto::HMAC::HashAlgorithm::SHA256);
  EXPECT_TRUE(
      hmac.Init(ByteVectorToString(kExpectedDerivedSymmetricKey16Bytes)));
  EXPECT_TRUE(hmac.Verify(kTestPayload, *key_proof));
}

TEST(DeviceSyncCryptAuthKeyProofComputerImplTest,
     AsymmetricKeyProofComputation_InvalidPrivateKeyFormat) {
  CryptAuthKey key("public_key", "non_pkcs8_private_key",
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256);

  std::optional<std::string> key_proof =
      CryptAuthKeyProofComputerImpl::Factory::Create()->ComputeKeyProof(
          key, kTestPayload, kAsymmetricTestSalt, std::nullopt /* info */);

  EXPECT_FALSE(key_proof);
}

}  // namespace device_sync

}  // namespace ash
