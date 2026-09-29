// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/crypto/fcm_decryption_result.h"

#include "base/notreached.h"

namespace fcm {

std::string ToFcmDecryptionResultDetailsString(FcmDecryptionResult result) {
  switch (result) {
    case FcmDecryptionResult::UNENCRYPTED:
      return "Message was not encrypted";
    case FcmDecryptionResult::DECRYPTED_DRAFT_03:
      return "Message decrypted (draft 03)";
    case FcmDecryptionResult::DECRYPTED_DRAFT_08:
      return "Message decrypted (draft 08)";
    case FcmDecryptionResult::INVALID_ENCRYPTION_HEADER:
      return "Invalid format for the Encryption header";
    case FcmDecryptionResult::INVALID_CRYPTO_KEY_HEADER:
      return "Invalid format for the Crypto-Key header";
    case FcmDecryptionResult::NO_KEYS:
      return "There are no associated keys with the subscription";
    case FcmDecryptionResult::INVALID_SHARED_SECRET:
      return "The shared secret cannot be derived from the keying material";
    case FcmDecryptionResult::INVALID_PAYLOAD:
      return "AES-GCM decryption failed";
    case FcmDecryptionResult::INVALID_BINARY_HEADER_PAYLOAD_LENGTH:
      return "The message payload is smaller than the smallest valid message "
             "(104 bytes)";
    case FcmDecryptionResult::INVALID_BINARY_HEADER_RECORD_SIZE:
      return "The record size indicated in the binary message header is "
             "smaller than the smallest valid record size (18 bytes)";
    case FcmDecryptionResult::INVALID_BINARY_HEADER_PUBLIC_KEY_LENGTH:
      return "The public key included in the binary message header must be a "
             "valid P-256 ECDH uncompressed point that is 65 bytes in length.";
    case FcmDecryptionResult::INVALID_BINARY_HEADER_PUBLIC_KEY_FORMAT:
      return "The public key included in the binary message header must be a "
             "valid P-256 ECDH uncompressed poin that starts with an 0x04 "
             "byte.";
    case FcmDecryptionResult::ENUM_SIZE:
      break;  // deliberate fall-through
  }

  NOTREACHED();
}

}  // namespace fcm
