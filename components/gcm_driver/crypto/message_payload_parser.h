// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_MESSAGE_PAYLOAD_PARSER_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_MESSAGE_PAYLOAD_PARSER_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/check.h"

namespace gcm {

enum class GCMDecryptionResult;

// Parses and validates the binary message payload included in messages that
// are encrypted per draft-ietf-webpush-encryption-08:
//
// https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-08#section-2.1
//
// In summary, such messages start with a binary header block that includes the
// parameters needed to decrypt the content, other than the key. All content
// following this binary header is considered the ciphertext.
//
// +-----------+--------+-----------+-----------------+
// | salt (16) | rs (4) | idlen (1) | public_key (65) |
// +-----------+--------+-----------+-----------------+
//
// Specific to Web Push encryption, the `public_key` parameter of this header
// must be set to the ECDH public key of the sender. This is a point on the
// P-256 elliptic curve in uncompressed form, 65 bytes long starting with 0x04.
//
// https://tools.ietf.org/html/draft-ietf-webpush-encryption-08#section-3.1
class MessagePayloadParser {
 public:
  explicit MessagePayloadParser(std::string_view message);

  MessagePayloadParser(const MessagePayloadParser&) = delete;
  MessagePayloadParser& operator=(const MessagePayloadParser&) = delete;

  ~MessagePayloadParser();

  // Returns whether the parser represents a valid message.
  bool IsValid() const { return is_valid_; }

  // Returns the failure reason when the given payload could not be parsed. Must
  // only be called when IsValid() returns false.
  GCMDecryptionResult GetFailureReason() const {
    DCHECK(failure_reason_.has_value());
    return failure_reason_.value();
  }

  // Returns the 16-byte long salt for the message. Must only be called after
  // validity of the message has been verified.
  const std::string& salt() const {
    CHECK(is_valid_);
    return salt_;
  }

  // Returns the record size for the message. Must only be called after validity
  // of the message has been verified.
  uint32_t record_size() const {
    CHECK(is_valid_);
    return record_size_;
  }

  // Returns the sender's ECDH public key for the message. This will be a point
  // on the P-256 elliptic curve in uncompressed form. Must only be called after
  // validity of the message has been verified.
  const std::string& public_key() const {
    CHECK(is_valid_);
    return public_key_;
  }

  // Returns the ciphertext for the message. This will be at least the size of
  // a single record, which is 18 octets. Must only be called after validity of
  // the message has been verified.
  const std::string& ciphertext() const {
    CHECK(is_valid_);
    return ciphertext_;
  }

 private:
  bool is_valid_ = false;
  std::optional<GCMDecryptionResult> failure_reason_;

  std::string salt_;
  uint32_t record_size_ = 0;
  std::string public_key_;
  std::string ciphertext_;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_MESSAGE_PAYLOAD_PARSER_H_
