// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/attestation/verification_key_utils.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "crypto/signature_verifier.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace private_ai {

VerificationKey::VerificationKey(
    std::vector<uint8_t> public_key,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    OutputPrefixType output_prefix_type)
    : public_key(std::move(public_key)),
      algorithm(algorithm),
      output_prefix_type(output_prefix_type) {}
VerificationKey::~VerificationKey() = default;
VerificationKey::VerificationKey(VerificationKey&&) = default;

namespace {

// Helper to read a big-endian uint32_t from a span and advance the span.
std::optional<uint32_t> ReadUint32BigEndianAndAdvance(
    base::span<const uint8_t>& data) {
  if (data.size() < 4) {
    return std::nullopt;
  }
  const auto bytes = data.first<4>();
  uint32_t value = static_cast<uint32_t>(bytes[0]) << 24 |
                   static_cast<uint32_t>(bytes[1]) << 16 |
                   static_cast<uint32_t>(bytes[2]) << 8 |
                   static_cast<uint32_t>(bytes[3]);
  data = data.subspan(4u);
  return value;
}

}  // namespace

std::map<uint32_t, VerificationKey> LoadVerificationKeys(
    base::span<const ProcessedKey> processed_keys) {
  std::map<uint32_t, VerificationKey> key_map;

  for (const auto& key : processed_keys) {
    const uint32_t key_id = key.id;
    const OutputPrefixType prefix_type = key.output_prefix_type;
    std::string raw_public_key_str = base::StrCat(
        {"\x04", std::string_view(key.x, 32), std::string_view(key.y, 32)});
    base::span<const uint8_t> raw_public_key =
        base::as_bytes(base::span(raw_public_key_str));

    bssl::UniquePtr<EC_GROUP> group(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    CHECK(group) << "Failed to create EC_GROUP for key " << key_id;

    bssl::UniquePtr<EC_POINT> point(EC_POINT_new(group.get()));
    CHECK(point) << "Failed to create EC_POINT for key " << key_id;

    CHECK_EQ(EC_POINT_oct2point(group.get(), point.get(), raw_public_key.data(),
                                raw_public_key.size(), nullptr),
             1)
        << "Failed to convert raw bytes to EC_POINT for key " << key_id;

    bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new());
    CHECK(ec_key) << "Failed to create EC_KEY for key " << key_id;
    CHECK_EQ(EC_KEY_set_group(ec_key.get(), group.get()), 1)
        << "Failed to set EC_GROUP on EC_KEY for key " << key_id;
    CHECK_EQ(EC_KEY_set_public_key(ec_key.get(), point.get()), 1)
        << "Failed to set public key in EC_KEY for key " << key_id;

    bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
    CHECK(pkey) << "Failed to create EVP_PKEY for key " << key_id;
    CHECK_EQ(EVP_PKEY_set1_EC_KEY(pkey.get(), ec_key.get()), 1)
        << "Failed to set EVP_PKEY for key " << key_id;

    int spki_len = i2d_PUBKEY(pkey.get(), nullptr);
    CHECK_GT(spki_len, 0) << "Failed to get SPKI length for key " << key_id;

    std::vector<uint8_t> public_key_spki(spki_len);
    uint8_t* spki_ptr = public_key_spki.data();
    CHECK_EQ(i2d_PUBKEY(pkey.get(), &spki_ptr), spki_len)
        << "Failed to DER-encode SPKI for key " << key_id;

    key_map.emplace(
        key_id,
        VerificationKey(std::move(public_key_spki),
                        crypto::SignatureVerifier::ECDSA_SHA256, prefix_type));
  }

  return key_map;
}

std::optional<std::pair<uint32_t, base::span<const uint8_t>>>
ParseTinkSignature(base::span<const uint8_t> signature) {
  if (signature.empty()) {
    LOG(ERROR) << "Invalid Tink signature size.";
    return std::nullopt;
  }

  uint8_t prefix = signature[0];
  if (prefix != 0x01 && prefix != 0x00) {
    LOG(ERROR) << "Unsupported Tink signature prefix: "
               << static_cast<int>(prefix);
    return std::nullopt;
  }

  auto remaining_signature = signature.subspan(1u);
  std::optional<uint32_t> key_id_opt =
      ReadUint32BigEndianAndAdvance(remaining_signature);

  if (!key_id_opt) {
    LOG(ERROR) << "Could not read key id";
    return std::nullopt;
  }
  uint32_t key_id = *key_id_opt;

  return std::make_pair(key_id, remaining_signature);
}

}  // namespace private_ai
