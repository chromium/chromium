// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_utils.h"

namespace web_package {

base::expected<EcdsaP256PublicKey, std::string> EcdsaP256PublicKey::Create(
    base::span<const uint8_t> bytes) {
  if (bytes.size() != kLength) {
    return base::unexpected(
        base::StringPrintf("The ECDSA P-256 public key does not have the "
                           "correct length. Expected %zu "
                           "bytes, but received %zu bytes.",
                           kLength, bytes.size()));
  }

  auto key_bytes = bytes.first<kLength>();
  if (!internal::IsValidEcdsaP256PublicKey(key_bytes)) {
    return base::unexpected(
        "Unable to parse a valid ECDSA P-256 key from the given bytes.");
  }

  std::array<uint8_t, kLength> key;
  base::ranges::copy(key_bytes, key.begin());

  return EcdsaP256PublicKey(std::move(key));
}

EcdsaP256PublicKey::EcdsaP256PublicKey(std::array<uint8_t, kLength> bytes)
    : bytes_(std::move(bytes)) {}

}  // namespace web_package
