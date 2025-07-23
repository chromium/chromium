// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/primitives.h"

#include <array>
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

// TODO(https://issues.chromium.org/issues/431824286): use crypto/keyexchange.
bool ComputeSharedSecret(base::span<const uint8_t, kKeySize> peer_public_value,
                         base::span<uint8_t, kKeySize> shared_secret,
                         base::span<uint8_t, kKeySize> generated_public_value) {
  // Generate new pair of private key and public value.
  std::array<uint8_t, kKeySize> out_private_key;
  X25519_keypair(generated_public_value.data(), out_private_key.data());

  // Compute shared secret.
  return X25519(shared_secret.data(), out_private_key.data(),
                peer_public_value.data()) == 1;
}

bool ProduceSymmetricKey(base::span<const uint8_t, kKeySize> shared_secret,
                         base::span<uint8_t, kKeySize> symmetric_key) {
  // Produce symmetric key from shared secret using HKDF.
  // Since the original keys were only used once, no salt and context is needed.
  // Since the keys above are only used once, no salt and context is provided.
  return HKDF(symmetric_key.data(), symmetric_key.size(),
              /*digest=*/EVP_sha256(), shared_secret.data(),
              shared_secret.size(), /*salt=*/nullptr, /*salt_len=*/0,
              /*info=*/nullptr, /*info_len=*/0) == 1;
}

bool PerformSymmetricEncryption(base::span<const uint8_t, kKeySize> key,
                                std::string_view input_data,
                                std::string* output_data) {
  // Encrypt the data with symmetric key using AEAD interface.
  crypto::Aead aead(crypto::Aead::CHACHA20_POLY1305);
  CHECK_EQ(aead.KeyLength(), kKeySize);

  // Use the symmetric key for data encryption.
  aead.Init(key);

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

bool VerifySignature(base::span<const uint8_t, kKeySize> verification_key,
                     std::string_view message,
                     base::span<const uint8_t, kSignatureSize> signature) {
  // Verify message
  base::span<const uint8_t> message_span = base::as_byte_span(message);
  return ED25519_verify(message_span.data(), message_span.size(),
                        signature.data(), verification_key.data()) == 1;
}

}  // namespace reporting
