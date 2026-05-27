// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/message_payload_parser.h"

#include <string_view>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_view_util.h"
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

MessagePayloadParser::MessagePayloadParser(std::string_view message_view) {
  auto message = base::as_byte_span(message_view);
  if (message.size() < kMinimumMessageSize) {
    failure_reason_ = GCMDecryptionResult::INVALID_BINARY_HEADER_PAYLOAD_LENGTH;
    return;
  }

  base::SpanReader reader(message);

  // We know these reads will succeed because we checked kMinimumMessageSize.
  salt_ = std::string(base::as_string_view(*reader.Read<kSaltSize>()));

  record_size_ = *reader.ReadU32BigEndian();

  if (record_size_ < kMinimumRecordSize) {
    failure_reason_ = GCMDecryptionResult::INVALID_BINARY_HEADER_RECORD_SIZE;
    return;
  }

  uint8_t public_key_length = *reader.ReadU8BigEndian();

  if (public_key_length != kUncompressedPointSize) {
    failure_reason_ =
        GCMDecryptionResult::INVALID_BINARY_HEADER_PUBLIC_KEY_LENGTH;
    return;
  }

  auto public_key = *reader.Read<kUncompressedPointSize>();
  if (public_key[0] != 0x04) {
    failure_reason_ =
        GCMDecryptionResult::INVALID_BINARY_HEADER_PUBLIC_KEY_FORMAT;
    return;
  }
  public_key_ = std::string(base::as_string_view(public_key));

  ciphertext_ = std::string(base::as_string_view(reader.remaining_span()));
  DCHECK_GE(ciphertext_.size(), kMinimumRecordSize);

  is_valid_ = true;
}

MessagePayloadParser::~MessagePayloadParser() = default;

}  // namespace gcm
