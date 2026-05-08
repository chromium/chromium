// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_token.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"

namespace private_verification_tokens {

PrivateVerificationTokensToken::PrivateVerificationTokensToken(
    std::string etld_plus_one,
    SerializedToken token,
    uint32_t key_id,
    base::Time expiration,
    uint32_t version)
    : etld_plus_one_(std::move(etld_plus_one)),
      token_(std::move(token)),
      key_id_(key_id),
      expiration_(expiration),
      version_(version) {}

PrivateVerificationTokensToken::PrivateVerificationTokensToken(
    const PrivateVerificationTokensToken&) = default;

PrivateVerificationTokensToken& PrivateVerificationTokensToken::operator=(
    const PrivateVerificationTokensToken&) = default;

PrivateVerificationTokensToken::PrivateVerificationTokensToken(
    PrivateVerificationTokensToken&&) = default;

PrivateVerificationTokensToken& PrivateVerificationTokensToken::operator=(
    PrivateVerificationTokensToken&&) = default;

PrivateVerificationTokensToken::~PrivateVerificationTokensToken() = default;

const std::string& PrivateVerificationTokensToken::etld_plus_one() const {
  return etld_plus_one_;
}

const SerializedToken& PrivateVerificationTokensToken::token() const {
  return token_;
}

uint32_t PrivateVerificationTokensToken::key_id() const {
  return key_id_;
}

base::Time PrivateVerificationTokensToken::expiration() const {
  return expiration_;
}

uint32_t PrivateVerificationTokensToken::version() const {
  return version_;
}

}  // namespace private_verification_tokens
