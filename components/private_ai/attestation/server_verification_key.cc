// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/attestation/server_verification_key.h"

#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "components/private_ai/features.h"

namespace private_ai {

namespace {

#include "components/private_ai/attestation/server_verification_key_data.inc"

}  // namespace

bool ProcessedKey::operator==(const ProcessedKey& other) const {
  constexpr size_t kKeyCoordinateSize = 32;
  return id == other.id && output_prefix_type == other.output_prefix_type &&
         std::string_view(x, kKeyCoordinateSize) ==
             std::string_view(other.x, kKeyCoordinateSize) &&
         std::string_view(y, kKeyCoordinateSize) ==
             std::string_view(other.y, kKeyCoordinateSize);
}

base::span<const ProcessedKey> GetServerVerificationKey(const GURL& url) {
  std::string_view host = url.host();

  if (base::StartsWith(host, "autopush")) {
    return kAutopushServerVerificationKeys;
  }
  if (base::StartsWith(host, "dev")) {
    return kDevServerVerificationKeys;
  }
  if (base::StartsWith(host, "staging")) {
    return kStagingServerVerificationKeys;
  }
  return kProdServerVerificationKeys;
}

base::span<const ProcessedKey> GetAutopushKeysForTesting() {
  return kAutopushServerVerificationKeys;
}

base::span<const ProcessedKey> GetDevKeysForTesting() {
  return kDevServerVerificationKeys;
}

base::span<const ProcessedKey> GetProdKeysForTesting() {
  return kProdServerVerificationKeys;
}

base::span<const ProcessedKey> GetStagingKeysForTesting() {
  return kStagingServerVerificationKeys;
}

}  // namespace private_ai
