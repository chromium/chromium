// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/rfc8188_util.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "crypto/keypair.h"
#include "crypto/random.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kInputMessage[] =
    "Hello, this is a test message for RFC 8188 payload encryption!";
const char kAuthSecret[] = "1234567890123456";

class Rfc8188UtilTest : public testing::Test {
 public:
  Rfc8188UtilTest() {
    recipient_private_key_ = crypto::keypair::PrivateKey::GenerateEcP256();
    std::vector<uint8_t> temp_recip_pub =
        recipient_private_key_->ToUncompressedX962Point();
    recipient_public_key_ =
        std::string(temp_recip_pub.begin(), temp_recip_pub.end());

    sender_private_key_ = crypto::keypair::PrivateKey::GenerateEcP256();
    std::vector<uint8_t> temp_sender_pub =
        sender_private_key_->ToUncompressedX962Point();
    sender_public_key_ =
        std::string(temp_sender_pub.begin(), temp_sender_pub.end());
  }

 protected:
  const crypto::keypair::PrivateKey& recipient_private_key() const {
    return *recipient_private_key_;
  }
  const std::string& recipient_public_key() const {
    return recipient_public_key_;
  }
  const crypto::keypair::PrivateKey& sender_private_key() const {
    return *sender_private_key_;
  }
  const std::string& sender_public_key() const { return sender_public_key_; }

 private:
  std::optional<crypto::keypair::PrivateKey> recipient_private_key_;
  std::string recipient_public_key_;
  std::optional<crypto::keypair::PrivateKey> sender_private_key_;
  std::string sender_public_key_;
};

TEST_F(Rfc8188UtilTest, SuccessRoundTrip) {
  std::string message = kInputMessage;
  std::string auth_secret = kAuthSecret;

  base::expected<std::string, Rfc8188EncryptionError> encrypted =
      EncryptPayloadWithRfc8188(message, recipient_public_key(), auth_secret,
                                sender_private_key());

  ASSERT_TRUE(encrypted.has_value());
  EXPECT_GT(encrypted->size(), message.size());
  ASSERT_GE(encrypted->size(), 16u + 4u + 1u);

  // Extract and verify fields from the encrypted message.
  std::string salt = encrypted->substr(0, 16);
  uint32_t rs =
      base::U32FromBigEndian(base::as_byte_span(*encrypted).subspan<16, 4>());
  uint8_t idlen = static_cast<uint8_t>((*encrypted)[20]);
  EXPECT_EQ(idlen, sender_public_key().size());
  std::string extracted_sender_public_key = encrypted->substr(21, idlen);
  EXPECT_EQ(extracted_sender_public_key, sender_public_key());
  std::string ciphertext = encrypted->substr(21 + idlen);

  std::string shared_secret;
  ASSERT_TRUE(ComputeSharedP256Secret(recipient_private_key(),
                                      sender_public_key(), &shared_secret));

  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_08);

  std::string decrypted_plaintext;
  bool success = cryptographer.Decrypt(
      recipient_public_key(), sender_public_key(), shared_secret, auth_secret,
      salt, ciphertext, rs, &decrypted_plaintext);

  ASSERT_TRUE(success);
  EXPECT_EQ(decrypted_plaintext, message);
}

TEST_F(Rfc8188UtilTest, SuccessRoundTripLargePayload) {
  std::string message(5000, 'a');
  std::string auth_secret = kAuthSecret;

  base::expected<std::string, Rfc8188EncryptionError> encrypted =
      EncryptPayloadWithRfc8188(message, recipient_public_key(), auth_secret,
                                sender_private_key());

  ASSERT_TRUE(encrypted.has_value());
  EXPECT_GT(encrypted->size(), message.size());
  ASSERT_GE(encrypted->size(), 16u + 4u + 1u);

  // Extract and verify fields from the encrypted message.
  std::string salt = encrypted->substr(0, 16);
  uint32_t rs =
      base::U32FromBigEndian(base::as_byte_span(*encrypted).subspan<16, 4>());
  uint8_t idlen = static_cast<uint8_t>((*encrypted)[20]);
  EXPECT_EQ(idlen, sender_public_key().size());
  std::string extracted_sender_public_key = encrypted->substr(21, idlen);
  EXPECT_EQ(extracted_sender_public_key, sender_public_key());
  std::string ciphertext = encrypted->substr(21 + idlen);

  EXPECT_EQ(rs, ciphertext.size());
  EXPECT_GT(rs, 4096u);

  std::string shared_secret;
  ASSERT_TRUE(ComputeSharedP256Secret(recipient_private_key(),
                                      sender_public_key(), &shared_secret));

  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_08);

  std::string decrypted_plaintext;
  bool success = cryptographer.Decrypt(
      recipient_public_key(), sender_public_key(), shared_secret, auth_secret,
      salt, ciphertext, rs, &decrypted_plaintext);

  ASSERT_TRUE(success);
  EXPECT_EQ(decrypted_plaintext, message);
}

TEST_F(Rfc8188UtilTest, LargePayloadInvalidOutputLegacyFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kRfc8188StrictCompliance);

  std::string message(5000, 'a');
  std::string auth_secret = kAuthSecret;

  base::expected<std::string, Rfc8188EncryptionError> encrypted =
      EncryptPayloadWithRfc8188(message, recipient_public_key(), auth_secret,
                                sender_private_key());

  ASSERT_TRUE(encrypted.has_value());
  uint32_t rs =
      base::U32FromBigEndian(base::as_byte_span(*encrypted).subspan<16, 4>());
  uint8_t idlen = static_cast<uint8_t>((*encrypted)[20]);
  std::string ciphertext = encrypted->substr(21 + idlen);

  // Without the strict compliance fix, advertised rs does not account for the
  // 16-byte AEAD tag, making rs strictly smaller than ciphertext.size().
  EXPECT_EQ(rs, message.size() + 2u);
  EXPECT_LT(rs, ciphertext.size());
}

TEST_F(Rfc8188UtilTest, InvalidInputs) {
  std::string message = kInputMessage;
  std::string auth_secret = kAuthSecret;

  std::string invalid_pub_key(64, '0');
  base::expected<std::string, Rfc8188EncryptionError> result =
      EncryptPayloadWithRfc8188(message, invalid_pub_key, auth_secret,
                                sender_private_key());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Rfc8188EncryptionError::kEncryptionFailed);

  std::string invalid_format_pub_key = recipient_public_key();
  invalid_format_pub_key[0] = 0x00;
  result = EncryptPayloadWithRfc8188(message, invalid_format_pub_key,
                                     auth_secret, sender_private_key());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Rfc8188EncryptionError::kEncryptionFailed);

  std::string invalid_auth_secret = "too_short";
  result = EncryptPayloadWithRfc8188(message, recipient_public_key(),
                                     invalid_auth_secret, sender_private_key());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Rfc8188EncryptionError::kEncryptionFailed);
}

TEST_F(Rfc8188UtilTest, InvalidInputsLegacyFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kRfc8188StrictCompliance);

  std::string message = kInputMessage;
  std::string auth_secret = kAuthSecret;

  std::string invalid_pub_key(64, '0');
  base::expected<std::string, Rfc8188EncryptionError> result =
      EncryptPayloadWithRfc8188(message, invalid_pub_key, auth_secret,
                                sender_private_key());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Rfc8188EncryptionError::kKeyDerivationFailed);
}

}  // namespace

}  // namespace gcm
