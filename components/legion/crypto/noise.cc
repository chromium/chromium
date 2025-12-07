// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/crypto/noise.h"

#include <string.h>

#include "base/check_op.h"
#include "base/numerics/byte_conversions.h"
#include "components/legion/crypto/constants.h"
#include "crypto/aead.h"
#include "crypto/hash.h"
#include "crypto/kdf.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/nid.h"

namespace {

// HKDF2 implements the functions with the same name from Noise[1],
// specialized to the case where |num_outputs| is two.
//
// [1] https://www.noiseprotocol.org/noise.html#hash-functions
std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>> HKDF2(
    base::span<const uint8_t, 32> ck,
    base::span<const uint8_t> ikm) {
  auto output = crypto::kdf::Hkdf<32 * 2>(crypto::hash::kSha256, ikm, ck,
                                          /*info=*/{});

  std::array<uint8_t, 32> a, b;
  auto [first, second] = base::span(output).split_at<32>();
  base::span(a).copy_from(first);
  base::span(b).copy_from(second);

  return std::make_tuple(a, b);
}

std::string_view ProtocolNameForHandshakeType(
    legion::Noise::HandshakeType type) {
  static const std::string_view kKNProtocolName =
      "Noise_KNpsk0_P256_AESGCM_SHA256";
  static const std::string_view kNKProtocolName =
      "Noise_NKpsk0_P256_AESGCM_SHA256";
  static const std::string_view kNKNoPskProtocolName =
      "Noise_NK_P256_AESGCM_SHA256";
  static const std::string_view kNNProtocolName = "Noise_NN_P256_AESGCM_SHA256";

  switch (type) {
    case legion::Noise::HandshakeType::kNKpsk0:
      return kNKProtocolName;
    case legion::Noise::HandshakeType::kKNpsk0:
      return kKNProtocolName;
    case legion::Noise::HandshakeType::kNK:
      return kNKNoPskProtocolName;
    case legion::Noise::HandshakeType::kNN:
      return kNNProtocolName;
  }
}

}  // namespace

namespace legion {

Noise::Noise() = default;
Noise::~Noise() = default;

void Noise::Init(Noise::HandshakeType type) {
  // See https://www.noiseprotocol.org/noise.html#the-handshakestate-object
  std::string_view name = ProtocolNameForHandshakeType(type);

  chaining_key_.fill(0);
  base::span(chaining_key_).copy_prefix_from(base::as_byte_span(name));
  h_ = chaining_key_;
}

void Noise::MixHash(base::span<const uint8_t> in) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  crypto::hash::Hasher hash(crypto::hash::kSha256);
  hash.Update(h_);
  hash.Update(in);
  hash.Finish(h_);
}

void Noise::MixKey(base::span<const uint8_t> ikm) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  std::array<uint8_t, 32> temp_k;
  std::tie(chaining_key_, temp_k) = HKDF2(chaining_key_, ikm);
  InitializeKey(temp_k);
}

void Noise::MixKeyAndHash(base::span<const uint8_t> ikm) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  auto output = crypto::kdf::Hkdf<32 * 3>(crypto::hash::kSha256, ikm,
                                          chaining_key_, /*info=*/{});
  base::span(chaining_key_).copy_from(base::span(output).first<32>());
  const auto [hash, key] = base::span(output).subspan<32>().split_at<32>();
  MixHash(hash);
  InitializeKey(key);
}

std::vector<uint8_t> Noise::EncryptAndHash(
    base::span<const uint8_t> plaintext) {
  uint8_t nonce[12] = {};
  base::span(nonce).first<4u>().copy_from(
      base::U32ToBigEndian(symmetric_nonce_));
  symmetric_nonce_++;

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(symmetric_key_);
  std::vector<uint8_t> ciphertext = aead.Seal(plaintext, nonce, h_);
  MixHash(ciphertext);
  return ciphertext;
}

std::optional<std::vector<uint8_t>> Noise::DecryptAndHash(
    base::span<const uint8_t> ciphertext) {
  uint8_t nonce[12] = {};
  base::span(nonce).first<4u>().copy_from(
      base::U32ToBigEndian(symmetric_nonce_));
  symmetric_nonce_++;

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(symmetric_key_);
  auto plaintext = aead.Open(ciphertext, nonce, h_);
  if (plaintext) {
    MixHash(ciphertext);
  }
  return plaintext;
}

std::array<uint8_t, 32> Noise::handshake_hash() const {
  return h_;
}

void Noise::MixHashPoint(const EC_POINT* point) {
  uint8_t x962[kP256X962Length];
  CHECK_EQ(sizeof(x962), EC_POINT_point2oct(EC_group_p256(), point,
                                            POINT_CONVERSION_UNCOMPRESSED, x962,
                                            sizeof(x962), /*ctx=*/nullptr));
  MixHash(x962);
}

std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>>
Noise::traffic_keys() const {
  return HKDF2(chaining_key_, {});
}

void Noise::InitializeKey(base::span<const uint8_t, 32> key) {
  // See https://www.noiseprotocol.org/noise.html#the-cipherstate-object
  base::span(symmetric_key_).copy_from(key);
  symmetric_nonce_ = 0;
}

}  // namespace legion
