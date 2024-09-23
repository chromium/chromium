// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace syncer {

CrossUserSharingPublicPrivateKeyPair::CrossUserSharingPublicPrivateKeyPair(
    CrossUserSharingPublicPrivateKeyPair&& other) = default;

CrossUserSharingPublicPrivateKeyPair&
CrossUserSharingPublicPrivateKeyPair::operator=(
    CrossUserSharingPublicPrivateKeyPair&& other) = default;

CrossUserSharingPublicPrivateKeyPair::~CrossUserSharingPublicPrivateKeyPair() =
    default;

// static
CrossUserSharingPublicPrivateKeyPair
CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair() {
  return CrossUserSharingPublicPrivateKeyPair();
}

// static
std::optional<CrossUserSharingPublicPrivateKeyPair>
CrossUserSharingPublicPrivateKeyPair::CreateByImport(
    base::span<const uint8_t> private_key) {
  if (private_key.size() != X25519_PRIVATE_KEY_LEN) {
    return std::nullopt;
  }
  return CrossUserSharingPublicPrivateKeyPair(private_key);
}

CrossUserSharingPublicPrivateKeyPair::CrossUserSharingPublicPrivateKeyPair(
    base::span<const uint8_t> private_key) {
  CHECK_EQ(static_cast<size_t>(X25519_PRIVATE_KEY_LEN), private_key.size());
  CHECK(EVP_HPKE_KEY_init(key_.get(), EVP_hpke_x25519_hkdf_sha256(),
                          private_key.data(), X25519_PRIVATE_KEY_LEN));
}

CrossUserSharingPublicPrivateKeyPair::CrossUserSharingPublicPrivateKeyPair() {
  CHECK(EVP_HPKE_KEY_generate(key_.get(), EVP_hpke_x25519_hkdf_sha256()));
}

std::array<uint8_t, X25519_PRIVATE_KEY_LEN>
CrossUserSharingPublicPrivateKeyPair::GetRawPrivateKey() const {
  std::array<uint8_t, X25519_PRIVATE_KEY_LEN> raw_private_key;
  size_t out_len;
  CHECK(EVP_HPKE_KEY_private_key(key_.get(), raw_private_key.data(), &out_len,
                                 raw_private_key.size()));
  CHECK_EQ(out_len, static_cast<size_t>(X25519_PRIVATE_KEY_LEN));
  return raw_private_key;
}

std::array<uint8_t, X25519_PUBLIC_VALUE_LEN>
CrossUserSharingPublicPrivateKeyPair::GetRawPublicKey() const {
  std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> raw_public_key;
  size_t out_len;
  CHECK(EVP_HPKE_KEY_public_key(key_.get(), raw_public_key.data(), &out_len,
                                raw_public_key.size()));
  CHECK_EQ(out_len, static_cast<size_t>(X25519_PUBLIC_VALUE_LEN));
  return raw_public_key;
}

std::optional<std::vector<uint8_t>>
CrossUserSharingPublicPrivateKeyPair::HpkeAuthEncrypt(
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> recipient_public_key,
    base::span<const uint8_t> authenticated_info) const {
  bssl::ScopedEVP_HPKE_CTX sender_context;

  // This vector will hold the encapsulated shared secret "enc" followed by the
  // symmetrically encrypted ciphertext "ct".
  std::vector<uint8_t> encrypted_data(EVP_HPKE_MAX_ENC_LENGTH);
  size_t encapsulated_shared_secret_len;

  if (!EVP_HPKE_CTX_setup_auth_sender(
          /*ctx=*/sender_context.get(),
          /*out_enc=*/encrypted_data.data(),
          /*out_enc_len=*/&encapsulated_shared_secret_len,
          /*max_enc=*/encrypted_data.size(), key_.get(),
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/EVP_hpke_chacha20_poly1305(),
          /*peer_public_key=*/recipient_public_key.data(),
          /*peer_public_key_len=*/recipient_public_key.size(),
          /*info=*/authenticated_info.data(),
          /*info_len=*/authenticated_info.size())) {
    return std::nullopt;
  }
  encrypted_data.resize(encapsulated_shared_secret_len + plaintext.size() +
                        EVP_HPKE_CTX_max_overhead(sender_context.get()));

  base::span<uint8_t> ciphertext =
      base::make_span(encrypted_data).subspan(encapsulated_shared_secret_len);
  size_t ciphertext_len;

  if (!EVP_HPKE_CTX_seal(
          /*ctx=*/sender_context.get(), /*out=*/ciphertext.data(),
          /*out_len=*/&ciphertext_len,
          /*max_out_len=*/ciphertext.size(), /*in=*/plaintext.data(),
          /*in_len*/ plaintext.size(),
          /*ad=*/nullptr,
          /*ad_len=*/0)) {
    return std::nullopt;
  }
  encrypted_data.resize(encapsulated_shared_secret_len + ciphertext_len);

  return encrypted_data;
}

std::optional<std::vector<uint8_t>>
CrossUserSharingPublicPrivateKeyPair::HpkeAuthDecrypt(
    base::span<const uint8_t> encrypted_data,
    base::span<const uint8_t> sender_public_key,
    base::span<const uint8_t> authenticated_info) const {
  bssl::ScopedEVP_HPKE_CTX sender_context;

  if (encrypted_data.size() < X25519_PUBLIC_VALUE_LEN) {
    VLOG(1) << "Invalid size of encrypted data";
    return std::nullopt;
  }

  base::span<const uint8_t> enc =
      encrypted_data.first<X25519_PUBLIC_VALUE_LEN>();

  bssl::ScopedEVP_HPKE_CTX recipient_context;
  if (!EVP_HPKE_CTX_setup_auth_recipient(
          /*ctx=*/recipient_context.get(), /*key=*/key_.get(),
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/EVP_hpke_chacha20_poly1305(),
          /*enc=*/enc.data(), /*enc_len=*/enc.size(),
          /*info=*/authenticated_info.data(),
          /*info_len=*/authenticated_info.size(),
          /*peer_public_key=*/sender_public_key.data(),
          /*peer_public_key_len=*/sender_public_key.size())) {
    VLOG(1) << "Cross-user sharing decryption: setup auth recipient failed";
    return std::nullopt;
  }

  base::span<const uint8_t> ciphertext =
      encrypted_data.subspan(X25519_PUBLIC_VALUE_LEN);
  std::vector<uint8_t> plaintext(ciphertext.size());
  size_t plaintext_len;

  if (!EVP_HPKE_CTX_open(
          /*ctx=*/recipient_context.get(), /*out=*/plaintext.data(),
          /*out_len*/ &plaintext_len, /*max_out_len=*/plaintext.size(),
          /*in=*/ciphertext.data(), /*in_len=*/ciphertext.size(),
          /*ad=*/nullptr,
          /*ad_len=*/0)) {
    VLOG(1) << "Cross-user sharing decryption: HPKE decryption failed";
    return std::nullopt;
  }

  plaintext.resize(plaintext_len);
  return plaintext;
}

}  // namespace syncer
