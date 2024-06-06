// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/verification.h"

#include <cstdint>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/encryption/testing_primitives.h"
#include "components/reporting/util/status_macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::HasSubstr;
using testing::Ne;
using testing::TestParamInfo;
using testing::TestWithParam;
using testing::ValuesIn;

namespace reporting {
namespace {

class VerificationTest : public ::testing::Test {
 protected:
  VerificationTest() = default;
  void SetUp() override {
    // Generate new pair of private key and public value.
    test::GenerateSigningKeyPair(private_key_, public_value_);
  }

  uint8_t public_value_[kKeySize];
  uint8_t private_key_[kSignKeySize];
};

TEST_F(VerificationTest, SignAndVerify) {
  static constexpr char message[] = "ABCDEF 012345";

  // Sign the message.
  uint8_t signature[kSignatureSize];
  test::SignMessage(private_key_, message, signature);

  // Verify the signature.
  SignatureVerifier verifier(
      std::string(reinterpret_cast<const char*>(public_value_), kKeySize));
  EXPECT_OK(verifier.Verify(
      std::string(message, strlen(message)),
      std::string(reinterpret_cast<const char*>(signature), kSignatureSize)));
}

TEST_F(VerificationTest, SignAndFailBadSignature) {
  static constexpr char message[] = "ABCDEF 012345";

  // Sign the message.
  uint8_t signature[kSignatureSize];
  test::SignMessage(private_key_, message, signature);

  // Verify the signature - wrong length.
  SignatureVerifier verifier(
      std::string(reinterpret_cast<const char*>(public_value_), kKeySize));
  Status status =
      verifier.Verify(std::string(message, strlen(message)),
                      std::string(reinterpret_cast<const char*>(signature),
                                  kSignatureSize - 1));
  EXPECT_THAT(status.code(), Eq(error::FAILED_PRECONDITION));
  EXPECT_THAT(status.message(), HasSubstr("Wrong signature size"));

  // Verify the signature - mismatch.
  signature[0] = ~signature[0];
  status = verifier.Verify(
      std::string(message, strlen(message)),
      std::string(reinterpret_cast<const char*>(signature), kSignatureSize));
  EXPECT_THAT(status.code(), Eq(error::INVALID_ARGUMENT));
  EXPECT_THAT(status.message(), HasSubstr("Verification failed"));
}

TEST_F(VerificationTest, SignAndFailBadPublicKey) {
  static constexpr char message[] = "ABCDEF 012345";

  // Sign the message.
  uint8_t signature[kSignatureSize];
  test::SignMessage(private_key_, message, signature);

  // Verify the public key - wrong length.
  SignatureVerifier verifier(
      std::string(reinterpret_cast<const char*>(public_value_), kKeySize - 1));
  Status status = verifier.Verify(
      std::string(message, strlen(message)),
      std::string(reinterpret_cast<const char*>(signature), kSignatureSize));
  EXPECT_THAT(status.code(), Eq(error::FAILED_PRECONDITION));
  EXPECT_THAT(status.message(), HasSubstr("Wrong public key size"));

  // Verify the public key - mismatch.
  public_value_[0] = ~public_value_[0];
  SignatureVerifier verifier2(
      std::string(reinterpret_cast<const char*>(public_value_), kKeySize));
  status = verifier2.Verify(
      std::string(message, strlen(message)),
      std::string(reinterpret_cast<const char*>(signature), kSignatureSize));
  EXPECT_THAT(status.code(), Eq(error::INVALID_ARGUMENT));
  EXPECT_THAT(status.message(), HasSubstr("Verification failed"));
}

TEST_F(VerificationTest, ValidateFixedKey) {
  // |prod_data_to_sign| is signed on PROD server, producing
  // |prod_server_signature|.
  static constexpr uint8_t prod_data_to_sign[] = {
      0xB3, 0xF9, 0xA3, 0xCC, 0xEB, 0x42, 0x88, 0x6b, 0x3f, 0x7b, 0x93, 0xC3,
      0xD3, 0x61, 0x9C, 0x45, 0xB4, 0xD7, 0x4B, 0x7B, 0x4F, 0xA7, 0x1A, 0x29,
      0xE1, 0x95, 0x14, 0xA4, 0x8C, 0x21, 0x36, 0x9F, 0x34, 0xA7, 0x4A, 0x57};
  static constexpr uint8_t prod_server_signature[kSignatureSize] = {
      0x17, 0xA4, 0x18, 0xA3, 0x78, 0x7A, 0x75, 0x24, 0xD9, 0x81, 0x3D,
      0x9F, 0x17, 0x28, 0x40, 0xD8, 0xE7, 0x67, 0x88, 0x17, 0x44, 0x7C,
      0xC2, 0x1A, 0xE2, 0x73, 0xAC, 0xB1, 0x0B, 0xCE, 0x60, 0xBB, 0x30,
      0x58, 0xCE, 0xF6, 0x8E, 0x33, 0xB6, 0xC6, 0x18, 0x3C, 0xA7, 0xD4,
      0x38, 0x91, 0x90, 0x2C, 0xBC, 0xB9, 0x76, 0x3C, 0xFF, 0x6F, 0x84,
      0xEC, 0x2D, 0x1E, 0x73, 0x43, 0x1B, 0x75, 0x5E, 0x0E};

  std::string prod_data_string =
      std::string(reinterpret_cast<const char*>(prod_data_to_sign),
                  sizeof(prod_data_to_sign));
  std::string prod_signature_string = std::string(
      reinterpret_cast<const char*>(prod_server_signature), kSignatureSize);

  SignatureVerifier verifier(SignatureVerifier::VerificationKey());
  const auto status = verifier.Verify(prod_data_string, prod_signature_string);
  EXPECT_OK(status) << status;
}

}  // namespace
}  // namespace reporting
