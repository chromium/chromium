// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_DECRYPTION_RESULT_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_DECRYPTION_RESULT_H_

#include <string>

namespace gcm {

// Result of decrypting an incoming message. The values of these reasons must
// not be changed as they are being recorded using UMA. When adding a value,
// please update GCMDecryptionResult in //tools/metrics/histograms/enums.xml.
enum class GCMDecryptionResult {
  // The message had not been encrypted by the sender.
  UNENCRYPTED = 0,

  // The message had been encrypted by the sender, and could successfully be
  // decrypted for the registration it has been received for. The encryption
  // scheme used for the message was draft-ietf-webpush-encryption-03.
  DECRYPTED_DRAFT_03 = 1,

  // The contents of the Encryption HTTP header could not be parsed.
  INVALID_ENCRYPTION_HEADER = 2,

  // The contents of the Crypto-Key HTTP header could not be parsed.
  INVALID_CRYPTO_KEY_HEADER = 3,

  // No public/private key-pair was associated with the app_id.
  NO_KEYS = 4,

  // The shared secret cannot be derived from the keying material.
  INVALID_SHARED_SECRET = 5,

  // The payload could not be decrypted as AES-128-GCM.
  INVALID_PAYLOAD = 6,

  // Removed in favour of the more detailed reasons below (values 9-13).
  // INVALID_BINARY_HEADER = 7,

  // The message had been encrypted by the sender, and could successfully be
  // decrypted for the registration it has been received for. The encryption
  // scheme used for the message was draft-ietf-webpush-encryption-08.
  DECRYPTED_DRAFT_08 = 8,

  // The payload's length is smaller than the smallest valid message.
  INVALID_BINARY_HEADER_PAYLOAD_LENGTH = 9,

  // The payload's record size is smaller than the smallest valid record + 1.
  INVALID_BINARY_HEADER_RECORD_SIZE = 10,

  // The public key included in the payload does not have the length
  // corresponding to an uncompressed P-256 ECDH key (65 bytes).
  INVALID_BINARY_HEADER_PUBLIC_KEY_LENGTH = 11,

  // The public key included in the message does not adhere to the format of
  // an uncompressed P-256 ECDH key. (I.e. it must start with 0x04.)
  INVALID_BINARY_HEADER_PUBLIC_KEY_FORMAT = 12,

  // Should be one more than the otherwise highest value in this enumeration.
  ENUM_SIZE = INVALID_BINARY_HEADER_PUBLIC_KEY_FORMAT + 1
};

// Converts the GCMDecryptionResult value to a string that can be used to
// explain the issue on chrome://gcm-internals/.
std::string ToGCMDecryptionResultDetailsString(GCMDecryptionResult result);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_DECRYPTION_RESULT_H_
