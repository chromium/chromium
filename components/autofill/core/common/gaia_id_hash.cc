// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/gaia_id_hash.h"

#include "base/base64.h"
#include "crypto/sha2.h"

namespace autofill {

// static
GaiaIdHash GaiaIdHash::FromGaiaId(const std::string& gaia_id) {
  return FromBinary(crypto::SHA256HashString(gaia_id));
}

// static
GaiaIdHash GaiaIdHash::FromBinary(const std::string& gaia_id_hash) {
  return GaiaIdHash(gaia_id_hash);
}

// static
GaiaIdHash GaiaIdHash::FromBase64(const std::string& gaia_id_base64_hash) {
  std::string gaia_id_hash;
  base::Base64Decode(gaia_id_base64_hash, &gaia_id_hash);
  return FromBinary(gaia_id_hash);
}

GaiaIdHash::GaiaIdHash() = default;

GaiaIdHash::GaiaIdHash(const GaiaIdHash& other) = default;

GaiaIdHash::GaiaIdHash(GaiaIdHash&& other) = default;

GaiaIdHash::~GaiaIdHash() = default;

GaiaIdHash::GaiaIdHash(const std::string& gaia_id_hash)
    : gaia_id_hash_(gaia_id_hash) {}

std::string GaiaIdHash::ToBinary() const {
  return gaia_id_hash_;
}

std::string GaiaIdHash::ToBase64() const {
  std::string gaia_id_base64_hash;
  base::Base64Encode(gaia_id_hash_, &gaia_id_base64_hash);
  return gaia_id_base64_hash;
}

bool GaiaIdHash::IsValid() const {
  return gaia_id_hash_.size() == crypto::kSHA256Length;
}

bool operator<(const GaiaIdHash& lhs, const GaiaIdHash& rhs) {
  return lhs.gaia_id_hash_ < rhs.gaia_id_hash_;
}

bool operator==(const GaiaIdHash& lhs, const GaiaIdHash& rhs) {
  return lhs.gaia_id_hash_ == rhs.gaia_id_hash_;
}

bool operator!=(const GaiaIdHash& lhs, const GaiaIdHash& rhs) {
  return !(lhs == rhs);
}

GaiaIdHash& GaiaIdHash::operator=(const GaiaIdHash& form) = default;

GaiaIdHash& GaiaIdHash::operator=(GaiaIdHash&& form) = default;

}  // namespace autofill
