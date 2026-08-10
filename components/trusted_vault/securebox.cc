// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/securebox.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "crypto/aead.h"
#include "crypto/kdf.h"
#include "crypto/kex.h"
#include "crypto/keypair.h"
#include "crypto/openssl_util.h"
#include "crypto/random.h"

namespace trusted_vault {

namespace {

const size_t kP256FieldBytes = 32;
const size_t kAES128KeyLength = 16;
const size_t kNonceLength = 12;
const size_t kECPointLength = 65;
const size_t kVersionLength = 2;
const uint8_t kSecureBoxVersion[] = {0x02, 0};
const uint8_t kHkdfSalt[] = {'S', 'E', 'C', 'U',  'R', 'E',
                             'B', 'O', 'X', 0x02, 0};
const char kHkdfInfoWithPublicKey[] = "P256 HKDF-SHA-256 AES-128-GCM";
const char kHkdfInfoWithoutPublicKey[] = "SHARED HKDF-SHA-256 AES-128-GCM";
const crypto::aead::Algorithm kAeadAlgorithm = crypto::aead::AES_128_GCM;

// Concatenates spans in |bytes_spans|.
std::vector<uint8_t> ConcatBytes(
    const std::vector<base::span<const uint8_t>>& bytes_spans) {
  size_t total_size = 0;
  for (const base::span<const uint8_t>& span : bytes_spans) {
    total_size += span.size();
  }

  std::vector<uint8_t> result(total_size);
  auto output_it = result.begin();
  for (const base::span<const uint8_t>& span : bytes_spans) {
    output_it = std::ranges::copy(span, output_it).out;
  }
  return result;
}

// Computes a 16-byte shared AES-GCM secret. If |private_key| is not nullopt,
// first computes the EC-DH secret. Appends the |shared_secret|, and computes
// HKDF of that. |public_key| and |private_key| might be nullopt, but if either
// of them is not nullopt, other must be not nullopt as well. |shared_secret|
// may be empty.
std::array<uint8_t, kAES128KeyLength> SecureBoxComputeSecret(
    std::optional<crypto::keypair::PrivateKey> private_key,
    std::optional<crypto::keypair::PublicKey> public_key,
    base::span<const uint8_t> shared_secret) {
  DCHECK_EQ(private_key.has_value(), public_key.has_value());
  std::vector<uint8_t> dh_secret;
  std::string hkdf_info;
  if (private_key) {
    hkdf_info = kHkdfInfoWithPublicKey;
    dh_secret.resize(kP256FieldBytes);
    crypto::kex::EcdhP256(*public_key, *private_key,
                          base::span<uint8_t, kP256FieldBytes>(dh_secret));
  } else {
    hkdf_info = kHkdfInfoWithoutPublicKey;
  }

  std::vector<uint8_t> key_material = ConcatBytes({dh_secret, shared_secret});
  return crypto::kdf::Hkdf<kAES128KeyLength>(crypto::hash::kSha256,
                                             key_material, kHkdfSalt,
                                             base::as_byte_span(hkdf_info));
}

// |our_key_pair| and |their_public_key| might be nullopt, but if either of them
// is not nullopt, other must be not nullopt as well. |shared_secret|, |header|
// and |payload| may be empty.
std::vector<uint8_t> SecureBoxEncryptImpl(
    std::optional<crypto::keypair::PrivateKey> our_key_pair,
    std::optional<crypto::keypair::PublicKey> their_public_key,
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> payload) {
  DCHECK_EQ(our_key_pair.has_value(), their_public_key.has_value());
  std::array<uint8_t, kAES128KeyLength> secret =
      SecureBoxComputeSecret(our_key_pair, their_public_key, shared_secret);

  std::vector<uint8_t> nonce = crypto::RandBytesAsVector(kNonceLength);
  std::vector<uint8_t> ciphertext =
      crypto::aead::Seal(kAeadAlgorithm, secret, payload, nonce, header);

  std::vector<uint8_t> encoded_our_public_key;
  if (our_key_pair) {
    encoded_our_public_key = our_key_pair->ToUncompressedX962Point();
  }

  return ConcatBytes(
      {kSecureBoxVersion, encoded_our_public_key, nonce, ciphertext});
}

// |our_key_pair| may be nullopt. |shared_secret|, |header| and |payload| may be
// empty. Returns nullopt if decryption failed.
std::optional<std::vector<uint8_t>> SecureBoxDecryptImpl(
    std::optional<crypto::keypair::PrivateKey> our_key_pair,
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> encrypted_payload) {
  size_t min_payload_size = kVersionLength + kNonceLength;
  if (our_key_pair) {
    min_payload_size += kECPointLength;
  }

  if (encrypted_payload.size() < min_payload_size ||
      encrypted_payload[0] != kSecureBoxVersion[0] ||
      encrypted_payload[1] != kSecureBoxVersion[1]) {
    return std::nullopt;
  }

  size_t offset = kVersionLength;
  std::optional<crypto::keypair::PublicKey> their_public_key;
  if (our_key_pair) {
    their_public_key = crypto::keypair::PublicKey::FromEcP256Point(
        encrypted_payload.subspan(offset, kECPointLength));
    if (!their_public_key) {
      return std::nullopt;
    }
    offset += kECPointLength;
  }

  std::array<uint8_t, kAES128KeyLength> secret_key =
      SecureBoxComputeSecret(our_key_pair, their_public_key, shared_secret);

  base::span<const uint8_t> nonce =
      encrypted_payload.subspan(offset, kNonceLength);
  offset += kNonceLength;

  base::span<const uint8_t> ciphertext = encrypted_payload.subspan(offset);

  return crypto::aead::Open(kAeadAlgorithm, secret_key, ciphertext, nonce,
                            header);
}

}  // namespace

std::vector<uint8_t> SecureBoxSymmetricEncrypt(
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> payload) {
  return SecureBoxEncryptImpl(/*our_key_pair=*/std::nullopt,
                              /*their_public_key=*/std::nullopt, shared_secret,
                              header, payload);
}

std::optional<std::vector<uint8_t>> SecureBoxSymmetricDecrypt(
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> encrypted_payload) {
  return SecureBoxDecryptImpl(/*our_key_pair=*/std::nullopt, shared_secret,
                              header, encrypted_payload);
}

// static
std::unique_ptr<SecureBoxPublicKey> SecureBoxPublicKey::CreateByImport(
    base::span<const uint8_t> key_bytes) {
  std::optional<crypto::keypair::PublicKey> key =
      crypto::keypair::PublicKey::FromEcP256Point(key_bytes);
  if (!key) {
    return nullptr;
  }

  return base::WrapUnique(new SecureBoxPublicKey(*key));
}

SecureBoxPublicKey::SecureBoxPublicKey(crypto::keypair::PublicKey key,
                                       base::PassKey<SecureBoxKeyPair>)
    : SecureBoxPublicKey(key) {}

SecureBoxPublicKey::SecureBoxPublicKey(crypto::keypair::PublicKey key)
    : key_(key) {
  CHECK(key_.IsEcP256());
}

SecureBoxPublicKey::~SecureBoxPublicKey() = default;

std::vector<uint8_t> SecureBoxPublicKey::ExportToBytes() const {
  return key_.ToUncompressedX962Point();
}

std::vector<uint8_t> SecureBoxPublicKey::Encrypt(
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> payload) const {
  const crypto::keypair::PrivateKey our_key_pair =
      crypto::keypair::PrivateKey::GenerateEcP256();
  return SecureBoxEncryptImpl(our_key_pair, key_, shared_secret, header,
                              payload);
}

// static
std::unique_ptr<SecureBoxPrivateKey> SecureBoxPrivateKey::CreateByImport(
    base::span<const uint8_t> key_bytes) {
  std::optional<crypto::keypair::PrivateKey> key =
      crypto::keypair::PrivateKey::FromEcP256PrivateScalar(key_bytes);
  if (!key) {
    return nullptr;
  }
  return base::WrapUnique(new SecureBoxPrivateKey(*key));
}

SecureBoxPrivateKey::SecureBoxPrivateKey(crypto::keypair::PrivateKey key,
                                         base::PassKey<SecureBoxKeyPair>)
    : SecureBoxPrivateKey(key) {}

SecureBoxPrivateKey::SecureBoxPrivateKey(crypto::keypair::PrivateKey key)
    : key_(key) {
  CHECK(key_.IsEcP256());
}

SecureBoxPrivateKey::~SecureBoxPrivateKey() = default;

std::vector<uint8_t> SecureBoxPrivateKey::ExportToBytes() const {
  return base::ToVector(key_.ToEcP256PrivateScalar());
}

std::optional<std::vector<uint8_t>> SecureBoxPrivateKey::Decrypt(
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> encrypted_payload) const {
  return SecureBoxDecryptImpl(key_, shared_secret, header, encrypted_payload);
}

// static
std::unique_ptr<SecureBoxKeyPair> SecureBoxKeyPair::GenerateRandom() {
  return base::WrapUnique(
      new SecureBoxKeyPair(crypto::keypair::PrivateKey::GenerateEcP256()));
}

// static
std::unique_ptr<SecureBoxKeyPair> SecureBoxKeyPair::CreateByPrivateKeyImport(
    base::span<const uint8_t> private_key_bytes) {
  std::optional<crypto::keypair::PrivateKey> key =
      crypto::keypair::PrivateKey::FromEcP256PrivateScalar(private_key_bytes);
  if (!key) {
    return nullptr;
  }

  return base::WrapUnique(new SecureBoxKeyPair(*key));
}

SecureBoxKeyPair::SecureBoxKeyPair(crypto::keypair::PrivateKey private_key)
    : private_key_(private_key, base::PassKey<SecureBoxKeyPair>()),
      public_key_(crypto::keypair::PublicKey::FromPrivateKey(private_key),
                  base::PassKey<SecureBoxKeyPair>()) {}

SecureBoxKeyPair::~SecureBoxKeyPair() = default;

}  // namespace trusted_vault
