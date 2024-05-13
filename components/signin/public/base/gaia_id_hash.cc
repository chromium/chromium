// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/gaia_id_hash.h"

#include <string_view>
#include <utility>

#include "base/base64.h"
#include "crypto/sha2.h"

namespace signin {

// static
GaiaIdHash GaiaIdHash::FromGaiaId(std::string_view gaia_id) {
  return FromBinary(crypto::SHA256HashString(gaia_id));
}

// static
GaiaIdHash GaiaIdHash::FromBinary(std::string gaia_id_hash) {
  return GaiaIdHash(std::move(gaia_id_hash));
}

// static
GaiaIdHash GaiaIdHash::FromBase64(std::string_view gaia_id_base64_hash) {
  std::string gaia_id_hash;
  base::Base64Decode(gaia_id_base64_hash, &gaia_id_hash);
  return FromBinary(std::move(gaia_id_hash));
}

GaiaIdHash::GaiaIdHash() = default;

GaiaIdHash::GaiaIdHash(const GaiaIdHash& other) = default;
GaiaIdHash& GaiaIdHash::operator=(const GaiaIdHash& form) = default;

GaiaIdHash::GaiaIdHash(GaiaIdHash&& other) = default;

GaiaIdHash& GaiaIdHash::operator=(GaiaIdHash&& form) = default;

GaiaIdHash::~GaiaIdHash() = default;

GaiaIdHash::GaiaIdHash(std::string gaia_id_hash)
    : gaia_id_hash_(std::move(gaia_id_hash)) {}

std::string GaiaIdHash::ToBinary() const {
  return gaia_id_hash_;
}

std::string GaiaIdHash::ToBase64() const {
  return base::Base64Encode(gaia_id_hash_);
}

bool GaiaIdHash::IsValid() const {
  return gaia_id_hash_.size() == crypto::kSHA256Length;
}

}  // namespace signin
