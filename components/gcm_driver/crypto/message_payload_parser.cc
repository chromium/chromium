// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/message_payload_parser.h"

#include "base/big_endian.h"
#include "base/strings/string_piece.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"

namespace gcm {

namespace {

// Size, in bytes, of the salt included in the message header.
constexpr size_t kSaltSize = 16;

// Size, in bytes, of the uncompressed point included in the message header.
constexpr size_t kUncompressedPointSize = 65;

// Size, in bytes, of the smallest allowable record_size value.
constexpr size_t kMinimumRecordSize = 18;

// Size, in bytes, of an empty message with the minimum amount of padding.
constexpr size_t kMinimumMessageSize =
    kSaltSize + sizeof(uint32_t) + sizeof(uint8_t) + kUncompressedPointSize +
    kMinimumRecordSize;

}  // namespace

MessagePayloadParser::MessagePayloadParser(base::StringPiece message) {
  if (message.size() < kMinimumMessageSize) {
    failure_reason_ = GCMDecryptionResult::INVALID_BINARY_HEADER_PAYLOAD_LENGTH;
    return;
  }

  salt_ = std::string(message.substr(0, kSaltSize));
  message.remove_prefix(kSaltSize);

  base::ReadBigEndian(reinterpret_cast<const uint8_t*>(message.data()),
                      &record_size_);
  message.remove_prefix(sizeof(record_size_));

  if (record_size_ < kMinimumRecordSize) {
    failure_reason_ = GCMDecryptionResult::INVALID_BINARY_HEADER_RECORD_SIZE;
    return;
  }

  uint8_t public_key_length;
  base::ReadBigEndian(reinterpret_cast<const uint8_t*>(message.data()),
                      &public_key_length);
  message.remove_prefix(sizeof(public_key_length));

  if (public_key_length != kUncompressedPointSize) {
    failure_reason_ =
        GCMDecryptionResult::INVALID_BINARY_HEADER_PUBLIC_KEY_LENGTH;
    return;
  }

  if (message[0] != 0x04) {
    failure_reason_ =
        GCMDecryptionResult::INVALID_BINARY_HEADER_PUBLIC_KEY_FORMAT;
    return;
  }

  public_key_ = std::string(message.substr(0, kUncompressedPointSize));
  message.remove_prefix(kUncompressedPointSize);

  ciphertext_ = std::string(message);
  DCHECK_GE(ciphertext_.size(), kMinimumRecordSize);

  is_valid_ = true;
}

MessagePayloadParser::~MessagePayloadParser() = default;

}  // namespace gcm
