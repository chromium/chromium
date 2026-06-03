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
#include "crypto/keypair.h"
#include "crypto/sign.h"

namespace private_ai {

VerificationKey::VerificationKey(crypto::keypair::PublicKey public_key,
                                 crypto::sign::SignatureKind algorithm,
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

    std::optional<crypto::keypair::PublicKey> public_key =
        crypto::keypair::PublicKey::FromEcP256Point(raw_public_key);
    CHECK(public_key) << "Failed to parse public key for key " << key_id;

    key_map.emplace(key_id,
                    VerificationKey(std::move(*public_key),
                                    crypto::sign::ECDSA_SHA256, prefix_type));
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
