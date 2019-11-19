// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_RESULT_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_RESULT_H_

#include <string>

namespace gcm {

// Result of encrypting an outgoing message. The values of these reasons must
// not be changed as they are being recorded using UMA. When adding a value,
// please update GCMEncryptionResult in //tools/metrics/histograms/enums.xml.
enum class GCMEncryptionResult {
  // The message had been successfully be encrypted. The encryption scheme used
  // for the message was draft-ietf-webpush-encryption-08.
  ENCRYPTED_DRAFT_08 = 0,

  // No public/private key-pair was associated with the app_id.
  NO_KEYS = 1,

  // The shared secret cannot be derived from the keying material.
  INVALID_SHARED_SECRET = 2,

  // The payload could not be encrypted as AES-128-GCM.
  ENCRYPTION_FAILED = 3,

  // Should be one more than the otherwise highest value in this enumeration.
  ENUM_SIZE = ENCRYPTION_FAILED + 1
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_RESULT_H_
