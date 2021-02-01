// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/verification.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

using ::testing::Eq;
using ::testing::HasSubstr;

namespace reporting {
namespace {

class VerificationTest : public ::testing::Test {
 protected:
  VerificationTest() = default;
  void SetUp() override {
    // Generate key pair
    ED25519_keypair(public_key_, private_key_);
  }

  uint8_t public_key_[ED25519_PUBLIC_KEY_LEN];
  uint8_t private_key_[ED25519_PRIVATE_KEY_LEN];
};

TEST_F(VerificationTest, SignAndVerify) {
  static constexpr char message[] = "ABCDEF 012345";
  // Sign a message.
  uint8_t signature[ED25519_SIGNATURE_LEN];
  ASSERT_THAT(ED25519_sign(signature, reinterpret_cast<const uint8_t*>(message),
                           strlen(message), private_key_),
              Eq(1));

  // Verify the signature.
  SignatureVerifier verifier(std::string(
      reinterpret_cast<const char*>(public_key_), ED25519_PUBLIC_KEY_LEN));
  EXPECT_OK(
      verifier.Verify(std::string(message, strlen(message)),
                      std::string(reinterpret_cast<const char*>(signature),
                                  ED25519_SIGNATURE_LEN)));
}

TEST_F(VerificationTest, SignAndFailBadSignature) {
  static constexpr char message[] = "ABCDEF 012345";
  // Sign a message.
  uint8_t signature[ED25519_SIGNATURE_LEN];
  ASSERT_THAT(ED25519_sign(signature, reinterpret_cast<const uint8_t*>(message),
                           strlen(message), private_key_),
              Eq(1));

  // Verify the signature - wrong length.
  SignatureVerifier verifier(std::string(
      reinterpret_cast<const char*>(public_key_), ED25519_PUBLIC_KEY_LEN));
  Status status =
      verifier.Verify(std::string(message, strlen(message)),
                      std::string(reinterpret_cast<const char*>(signature),
                                  ED25519_SIGNATURE_LEN - 1));
  EXPECT_THAT(status.code(), Eq(error::FAILED_PRECONDITION));
  EXPECT_THAT(status.message(), HasSubstr("Wrong signature size"));

  // Verify the signature - mismatch.
  signature[0] = ~signature[0];
  status = verifier.Verify(std::string(message, strlen(message)),
                           std::string(reinterpret_cast<const char*>(signature),
                                       ED25519_SIGNATURE_LEN));
  EXPECT_THAT(status.code(), Eq(error::INVALID_ARGUMENT));
  EXPECT_THAT(status.message(), HasSubstr("Verification failed"));
}

TEST_F(VerificationTest, SignAndFailBadPublicKey) {
  static constexpr char message[] = "ABCDEF 012345";
  // Sign a message.
  uint8_t signature[ED25519_SIGNATURE_LEN];
  ASSERT_THAT(ED25519_sign(signature, reinterpret_cast<const uint8_t*>(message),
                           strlen(message), private_key_),
              Eq(1));

  // Verify the public key - wrong length.
  SignatureVerifier verifier(std::string(
      reinterpret_cast<const char*>(public_key_), ED25519_PUBLIC_KEY_LEN - 1));
  Status status =
      verifier.Verify(std::string(message, strlen(message)),
                      std::string(reinterpret_cast<const char*>(signature),
                                  ED25519_SIGNATURE_LEN));
  EXPECT_THAT(status.code(), Eq(error::FAILED_PRECONDITION));
  EXPECT_THAT(status.message(), HasSubstr("Wrong public key size"));

  // Verify the public key - mismatch.
  public_key_[0] = ~public_key_[0];
  SignatureVerifier verifier2(std::string(
      reinterpret_cast<const char*>(public_key_), ED25519_PUBLIC_KEY_LEN));
  status =
      verifier2.Verify(std::string(message, strlen(message)),
                       std::string(reinterpret_cast<const char*>(signature),
                                   ED25519_SIGNATURE_LEN));
  EXPECT_THAT(status.code(), Eq(error::INVALID_ARGUMENT));
  EXPECT_THAT(status.message(), HasSubstr("Verification failed"));
}

TEST_F(VerificationTest, ValidateFixedKey) {
  // |dev_data_to_sign| is signed on DEV server, producing
  // |dev_server_signature|.
  static constexpr uint8_t dev_data_to_sign[] = {
      0x4D, 0x22, 0x5C, 0x4C, 0x74, 0x23, 0x82, 0x80, 0x58, 0xA2, 0x31, 0xA2,
      0xC6, 0xE2, 0x6D, 0xDA, 0x48, 0x82, 0x7A, 0x9C, 0xF7, 0xD0, 0x4A, 0xF2,
      0xFD, 0x19, 0x03, 0x7F, 0xC5, 0x6F, 0xBB, 0x49, 0xAF, 0x91, 0x7B, 0x74};
  static constexpr uint8_t dev_server_signature[ED25519_SIGNATURE_LEN] = {
      0x0C, 0xA4, 0xAF, 0xE3, 0x27, 0x06, 0xD1, 0x4F, 0x0E, 0x05, 0x44,
      0x74, 0x0D, 0x4F, 0xA0, 0x4C, 0x26, 0xB1, 0x0C, 0x44, 0x92, 0x0F,
      0x96, 0xAF, 0x5A, 0x7E, 0x45, 0xED, 0x61, 0xB7, 0x87, 0xA8, 0xA3,
      0x98, 0x52, 0x97, 0x8D, 0x56, 0xA3, 0xED, 0xF7, 0x9B, 0x54, 0x17,
      0x61, 0x32, 0x6C, 0x06, 0x29, 0xBF, 0x30, 0x4E, 0x88, 0x72, 0xAB,
      0xE3, 0x60, 0xDA, 0xF0, 0x37, 0xEB, 0x56, 0x28, 0x0B};

  // Validate the signature using known DEV public key.
  SignatureVerifier dev_verifier(SignatureVerifier::VerificationKeyDev());
  const auto dev_result = dev_verifier.Verify(
      std::string(reinterpret_cast<const char*>(dev_data_to_sign),
                  sizeof(dev_data_to_sign)),
      std::string(reinterpret_cast<const char*>(dev_server_signature),
                  ED25519_SIGNATURE_LEN));
  EXPECT_OK(dev_result) << dev_result;

  // |prod_data_to_sign| is signed on PROD server, producing
  // |prod_server_signature|.
  static constexpr uint8_t prod_data_to_sign[] = {
      0xB3, 0xF9, 0xA3, 0xCC, 0xEB, 0x42, 0x88, 0x6b, 0x3f, 0x7b, 0x93, 0xC3,
      0xD3, 0x61, 0x9C, 0x45, 0xB4, 0xD7, 0x4B, 0x7B, 0x4F, 0xA7, 0x1A, 0x29,
      0xE1, 0x95, 0x14, 0xA4, 0x8C, 0x21, 0x36, 0x9F, 0x34, 0xA7, 0x4A, 0x57};
  static constexpr uint8_t prod_server_signature[ED25519_SIGNATURE_LEN] = {
      0x17, 0xA4, 0x18, 0xA3, 0x78, 0x7A, 0x75, 0x24, 0xD9, 0x81, 0x3D,
      0x9F, 0x17, 0x28, 0x40, 0xD8, 0xE7, 0x67, 0x88, 0x17, 0x44, 0x7C,
      0xC2, 0x1A, 0xE2, 0x73, 0xAC, 0xB1, 0x0B, 0xCE, 0x60, 0xBB, 0x30,
      0x58, 0xCE, 0xF6, 0x8E, 0x33, 0xB6, 0xC6, 0x18, 0x3C, 0xA7, 0xD4,
      0x38, 0x91, 0x90, 0x2C, 0xBC, 0xB9, 0x76, 0x3C, 0xFF, 0x6F, 0x84,
      0xEC, 0x2D, 0x1E, 0x73, 0x43, 0x1B, 0x75, 0x5E, 0x0E};

  // Validate the signature using known PROD public key.
  SignatureVerifier prod_verifier(SignatureVerifier::VerificationKey());
  const auto prod_result = prod_verifier.Verify(
      std::string(reinterpret_cast<const char*>(prod_data_to_sign),
                  sizeof(prod_data_to_sign)),
      std::string(reinterpret_cast<const char*>(prod_server_signature),
                  ED25519_SIGNATURE_LEN));
  EXPECT_OK(prod_result) << prod_result;
}
}  // namespace
}  // namespace reporting
