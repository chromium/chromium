// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/verification.h"

#include "components/reporting/util/status.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace reporting {

namespace {

// Well-known public signature verification keys.
constexpr uint8_t kProdVerificationKey[ED25519_PUBLIC_KEY_LEN] = {
    0x51, 0x2D, 0x53, 0xA3, 0xF5, 0xEB, 0x01, 0xCE, 0xDA, 0xFC, 0x8E,
    0x79, 0xE7, 0x0F, 0xE1, 0x65, 0xDC, 0x14, 0x86, 0x53, 0x8B, 0x97,
    0x5A, 0x2D, 0x70, 0x08, 0xCB, 0xCA, 0x60, 0xC3, 0x55, 0xE6};

constexpr uint8_t kDevVerificationKey[ED25519_PUBLIC_KEY_LEN] = {
    0xC6, 0x2C, 0x4D, 0x25, 0x9E, 0x3E, 0x99, 0xA0, 0x2E, 0x08, 0x15,
    0x8C, 0x38, 0xB7, 0x6C, 0x08, 0xDF, 0xE7, 0x6E, 0x3A, 0xD6, 0x5A,
    0xC5, 0x58, 0x09, 0xE4, 0xAB, 0x89, 0x3A, 0x31, 0x53, 0x07};

}  // namespace

// static
base::StringPiece SignatureVerifier::VerificationKey() {
  return base::StringPiece(reinterpret_cast<const char*>(kProdVerificationKey),
                           ED25519_PUBLIC_KEY_LEN);
}

// static
base::StringPiece SignatureVerifier::VerificationKeyDev() {
  return base::StringPiece(reinterpret_cast<const char*>(kDevVerificationKey),
                           ED25519_PUBLIC_KEY_LEN);
}

SignatureVerifier::SignatureVerifier(base::StringPiece verification_public_key)
    : verification_public_key_(verification_public_key) {}

Status SignatureVerifier::Verify(base::StringPiece message,
                                 base::StringPiece signature) {
  if (signature.size() != ED25519_SIGNATURE_LEN) {
    return Status{error::FAILED_PRECONDITION, "Wrong signature size"};
  }
  if (verification_public_key_.size() != ED25519_PUBLIC_KEY_LEN) {
    return Status{error::FAILED_PRECONDITION, "Wrong public key size"};
  }
  const int result = ED25519_verify(
      reinterpret_cast<const uint8_t*>(message.data()), message.size(),
      reinterpret_cast<const uint8_t*>(signature.data()),
      reinterpret_cast<const uint8_t*>(verification_public_key_.data()));
  if (result != 1) {
    return Status{error::INVALID_ARGUMENT, "Verification failed"};
  }
  return Status::StatusOK();
}

}  // namespace reporting
