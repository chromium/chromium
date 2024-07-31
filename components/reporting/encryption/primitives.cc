// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/reporting/encryption/primitives.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "base/check_op.h"
#include "crypto/aead.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"


namespace reporting {

static_assert(X25519_PRIVATE_KEY_LEN == kKeySize, "X25519 mismatch");
static_assert(X25519_PUBLIC_VALUE_LEN == kKeySize, "X25519 mismatch");
static_assert(X25519_SHARED_KEY_LEN == kKeySize, "X25519 mismatch");
static_assert(ED25519_PRIVATE_KEY_LEN == kSignKeySize, "ED25519 mismatch");
static_assert(ED25519_PUBLIC_KEY_LEN == kKeySize, "ED25519 mismatch");
static_assert(ED25519_SIGNATURE_LEN == kSignatureSize, "ED25519 mismatch");

bool ComputeSharedSecret(const uint8_t peer_public_value[kKeySize],
                         uint8_t shared_secret[kKeySize],
                         uint8_t generated_public_value[kKeySize]) {
  // Generate new pair of private key and public value.
  uint8_t out_private_key[kKeySize];
  X25519_keypair(generated_public_value, out_private_key);

  // Compute shared secret.
  if (1 != X25519(shared_secret, out_private_key, peer_public_value)) {
    return false;
  }

  // Success.
  return true;
}

bool ProduceSymmetricKey(const uint8_t shared_secret[kKeySize],
                         uint8_t symmetric_key[kKeySize]) {
  // Produce symmetric key from shared secret using HKDF.
  // Since the original keys were only used once, no salt and context is needed.
  // Since the keys above are only used once, no salt and context is provided.
  if (1 != HKDF(symmetric_key, kKeySize, /*digest=*/EVP_sha256(), shared_secret,
                kKeySize,
                /*salt=*/nullptr, /*salt_len=*/0,
                /*info=*/nullptr, /*info_len=*/0)) {
    return false;
  }

  // Success.
  return true;
}

bool PerformSymmetricEncryption(const uint8_t symmetric_key[kKeySize],
                                std::string_view input_data,
                                std::string* output_data) {
  // Encrypt the data with symmetric key using AEAD interface.
  crypto::Aead aead(crypto::Aead::CHACHA20_POLY1305);
  CHECK_EQ(aead.KeyLength(), kKeySize);

  // Use the symmetric key for data encryption.
  aead.Init(base::make_span(symmetric_key, kKeySize));

  // Set nonce to 0s, since a symmetric key is only used once.
  // Note: if we ever start reusing the same symmetric key, we will need
  // to generate new nonce for every record and transfer it to the peer.
  CHECK_EQ(aead.NonceLength(), kNonceSize);
  std::string nonce(kNonceSize, 0);

  // Encrypt the whole record.
  if (1 != aead.Seal(input_data, nonce, std::string(), output_data)) {
    return false;
  }

  // Success. Attach nonce at the head, for compatibility with Tink.
  output_data->insert(0, nonce);
  return true;
}

bool VerifySignature(const uint8_t verification_key[kKeySize],
                     std::string_view message,
                     const uint8_t signature[kSignatureSize]) {
  // Verify message
  if (1 != ED25519_verify(reinterpret_cast<const uint8_t*>(message.data()),
                          message.size(), signature, verification_key)) {
    return false;
  }

  // Success.
  return true;
}

}  // namespace reporting
