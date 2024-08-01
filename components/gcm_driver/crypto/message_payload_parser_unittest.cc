// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gcm_driver/crypto/message_payload_parser.h"

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

constexpr size_t kSaltSize = 16;
constexpr size_t kPublicKeySize = 65;
constexpr size_t kCiphertextSize = 18;

const uint8_t kValidMessage[] = {
    // salt (16 bytes, kSaltSize)
    0x59, 0xFD, 0x35, 0x97, 0x3B, 0xF3, 0x66, 0xA7, 0xEB, 0x8D, 0x44, 0x1E,
    0xCB, 0x4D, 0xFC, 0xD8,
    // rs (4 bytes, in network byte order)
    0x00, 0x00, 0x00, 0x12,
    // idlen (1 byte)
    0x41,
    // public key (65 bytes, kPublicKeySize, must start with 0x04)
    0x04, 0x35, 0x02, 0x67, 0xB9, 0x10, 0x8F, 0x9B, 0xF1, 0x85, 0xF5, 0x1B,
    0xD7, 0xA4, 0xEF, 0xBD, 0x28, 0xB3, 0x11, 0x40, 0xBA, 0xD0, 0xEE, 0xB2,
    0x97, 0xDA, 0x6A, 0x93, 0x2D, 0x26, 0x45, 0xBD, 0xB2, 0x9A, 0x9F, 0xB8,
    0x19, 0xD8, 0x21, 0x6F, 0x66, 0xE3, 0xF6, 0x0B, 0x74, 0xB2, 0x28, 0x38,
    0xDC, 0xA7, 0x8A, 0x58, 0x0D, 0x56, 0x47, 0x3E, 0xD0, 0x5B, 0x5C, 0x93,
    0x4E, 0xB3, 0x89, 0x87, 0x64,
    // payload (18 bytes, kCiphertextSize)
    0x3F, 0xD8, 0x95, 0x2C, 0xA2, 0x11, 0xBD, 0x7B, 0x57, 0xB2, 0x00, 0xBD,
    0x57, 0x68, 0x3F, 0xF0, 0x14, 0x57};

static_assert(std::size(kValidMessage) == 104,
              "The smallest valid message is 104 bytes in size.");

// Creates an std::string for the |kValidMessage| constant.
std::string CreateMessageString() {
  return std::string(reinterpret_cast<const char*>(kValidMessage),
                     std::size(kValidMessage));
}

TEST(MessagePayloadParserTest, ValidMessage) {
  MessagePayloadParser parser(CreateMessageString());
  ASSERT_TRUE(parser.IsValid());

  const uint8_t* salt = kValidMessage;

  ASSERT_EQ(parser.salt().size(), kSaltSize);
  EXPECT_EQ(parser.salt(), std::string(salt, salt + kSaltSize));

  ASSERT_EQ(parser.record_size(), 18u);

  const uint8_t* public_key =
      kValidMessage + kSaltSize + sizeof(uint32_t) + sizeof(uint8_t);

  ASSERT_EQ(parser.public_key().size(), kPublicKeySize);
  EXPECT_EQ(parser.public_key(),
            std::string(public_key, public_key + kPublicKeySize));

  const uint8_t* ciphertext = kValidMessage + kSaltSize + sizeof(uint32_t) +
                              sizeof(uint8_t) + kPublicKeySize;

  ASSERT_EQ(parser.ciphertext().size(), kCiphertextSize);
  EXPECT_EQ(parser.ciphertext(),
            std::string(ciphertext, ciphertext + kCiphertextSize));
}

TEST(MessagePayloadParserTest, MinimumMessageSize) {
  std::string message = CreateMessageString();
  message.resize(std::size(kValidMessage) / 2);

  MessagePayloadParser parser(message);
  EXPECT_FALSE(parser.IsValid());
  EXPECT_EQ(parser.GetFailureReason(),
            GCMDecryptionResult::INVALID_BINARY_HEADER_PAYLOAD_LENGTH);
}

TEST(MessagePayloadParserTest, MinimumRecordSize) {
  std::string message = CreateMessageString();

  auto record_size_span = base::as_writable_byte_span(message).subspan(
      16u /* salt */, sizeof(uint32_t));
  const uint32_t invalid_record_size = 11u;
  record_size_span.copy_from(base::U32ToBigEndian(invalid_record_size));

  MessagePayloadParser parser(message);
  EXPECT_FALSE(parser.IsValid());
  EXPECT_EQ(parser.GetFailureReason(),
            GCMDecryptionResult::INVALID_BINARY_HEADER_RECORD_SIZE);
}

TEST(MessagePayloadParserTest, InvalidPublicKeyLength) {
  std::string message = CreateMessageString();

  auto pubkey_span = base::as_writable_byte_span(message).subspan(
      16u /* salt */ + 4u /* rs */, sizeof(uint8_t));
  uint8_t invalid_public_key_size = 42;
  pubkey_span.copy_from(base::U8ToBigEndian(invalid_public_key_size));

  MessagePayloadParser parser(message);
  EXPECT_FALSE(parser.IsValid());
  EXPECT_EQ(parser.GetFailureReason(),
            GCMDecryptionResult::INVALID_BINARY_HEADER_PUBLIC_KEY_LENGTH);
}

TEST(MessagePayloadParserTest, InvalidPublicKeyFormat) {
  std::string message = CreateMessageString();

  // Replace the first byte of the key, which signals the point format.
  message[16u /* salt */ + 4u /* rs */ + 1u /* idlen */] = 0x42;

  MessagePayloadParser parser(message);
  EXPECT_FALSE(parser.IsValid());
  EXPECT_EQ(parser.GetFailureReason(),
            GCMDecryptionResult::INVALID_BINARY_HEADER_PUBLIC_KEY_FORMAT);
}

}  // namespace

}  // namespace gcm
