// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "crypto/hash.h"

namespace private_verification_tokens {

PrivateVerificationTokensPublicKey::PrivateVerificationTokensPublicKey(
    url::Origin issuer,
    std::vector<uint8_t> public_key,
    std::vector<uint8_t> public_key_proof,
    base::Time expiration,
    uint32_t version)
    : issuer_(std::move(issuer)),
      public_key_(std::move(public_key)),
      public_key_proof_(std::move(public_key_proof)),
      key_id_(crypto::hash::Sha256(public_key_)),
      expiration_(expiration),
      version_(version) {}

PrivateVerificationTokensPublicKey::PrivateVerificationTokensPublicKey(
    const PrivateVerificationTokensPublicKey&) = default;

PrivateVerificationTokensPublicKey&
PrivateVerificationTokensPublicKey::operator=(
    const PrivateVerificationTokensPublicKey&) = default;

PrivateVerificationTokensPublicKey::PrivateVerificationTokensPublicKey(
    PrivateVerificationTokensPublicKey&&) = default;

PrivateVerificationTokensPublicKey&
PrivateVerificationTokensPublicKey::operator=(
    PrivateVerificationTokensPublicKey&&) = default;

PrivateVerificationTokensPublicKey::~PrivateVerificationTokensPublicKey() =
    default;

const url::Origin& PrivateVerificationTokensPublicKey::issuer() const {
  return issuer_;
}

const std::vector<uint8_t>& PrivateVerificationTokensPublicKey::public_key()
    const {
  return public_key_;
}

const std::vector<uint8_t>&
PrivateVerificationTokensPublicKey::public_key_proof() const {
  return public_key_proof_;
}

const std::array<uint8_t, crypto::hash::kSha256Size>&
PrivateVerificationTokensPublicKey::key_id() const {
  return key_id_;
}

uint8_t PrivateVerificationTokensPublicKey::truncated_key_id() const {
  return key_id_.back();
}

base::Time PrivateVerificationTokensPublicKey::expiration() const {
  return expiration_;
}

uint32_t PrivateVerificationTokensPublicKey::version() const {
  return version_;
}

}  // namespace private_verification_tokens
