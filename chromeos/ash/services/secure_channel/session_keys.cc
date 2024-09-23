// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/session_keys.h"

#include "crypto/hkdf.h"

namespace ash::secure_channel {

namespace {

// The salt used to compute the devired keys. SHA256 of "D2D".
const uint8_t kSalt[] = {0x82, 0xAA, 0x55, 0xA0, 0xD3, 0x97, 0xF8, 0x83,
                         0x46, 0xCA, 0x1C, 0xEE, 0x8D, 0x39, 0x09, 0xB9,
                         0x5F, 0x13, 0xFA, 0x7D, 0xEB, 0x1D, 0x4A, 0xB3,
                         0x83, 0x76, 0xB8, 0x25, 0x6D, 0xA8, 0x55, 0x10};

// The number of bytes in the derived keys.
const int kKeySize = 32;

// String used in the derivation of the inititor encoding key.
const char kInitiatorPurpose[] = "initiator";

// String used in the derivation of the responder encoding key.
const char kResponderPurpose[] = "responder";

}  // namespace

SessionKeys::SessionKeys(const std::string& session_symmetric_key) {
  std::string salt(reinterpret_cast<const char*>(&kSalt[0]), sizeof(kSalt));

  initiator_encode_key_ = crypto::HkdfSha256(session_symmetric_key, salt,
                                             kInitiatorPurpose, kKeySize);
  responder_encode_key_ = crypto::HkdfSha256(session_symmetric_key, salt,
                                             kResponderPurpose, kKeySize);
}

SessionKeys::SessionKeys() {}

SessionKeys::~SessionKeys() {}

std::string SessionKeys::initiator_encode_key() const {
  return initiator_encode_key_;
}

std::string SessionKeys::responder_encode_key() const {
  return responder_encode_key_;
}

}  // namespace ash::secure_channel
