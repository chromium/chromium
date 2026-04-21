// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"

#include <utility>

#include "base/time/time.h"

namespace private_verification_tokens {

PrivateVerificationTokensPublicKey::PrivateVerificationTokensPublicKey(
    std::string etld_plus_one,
    std::vector<uint8_t> public_key,
    uint32_t key_id,
    base::Time expiration,
    uint32_t version)
    : etld_plus_one_(std::move(etld_plus_one)),
      public_key_(std::move(public_key)),
      key_id_(key_id),
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

const std::string& PrivateVerificationTokensPublicKey::etld_plus_one() const {
  return etld_plus_one_;
}

const std::vector<uint8_t>& PrivateVerificationTokensPublicKey::public_key()
    const {
  return public_key_;
}

uint32_t PrivateVerificationTokensPublicKey::key_id() const {
  return key_id_;
}

base::Time PrivateVerificationTokensPublicKey::expiration() const {
  return expiration_;
}

uint32_t PrivateVerificationTokensPublicKey::version() const {
  return version_;
}

}  // namespace private_verification_tokens
