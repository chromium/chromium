// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_token.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/time/time.h"
#include "url/origin.h"

namespace private_verification_tokens {

PrivateVerificationTokensToken::PrivateVerificationTokensToken(
    url::Origin issuer,
    SerializedToken token,
    uint32_t key_id,
    base::Time expiration,
    uint32_t version,
    base::Time creation_time,
    bool is_proxied)
    : issuer_(std::move(issuer)),
      token_(std::move(token)),
      key_id_(key_id),
      expiration_(expiration),
      version_(version),
      creation_time_(
          base::Time::UnixEpoch() +
          base::Seconds((creation_time - base::Time::UnixEpoch()).InSeconds())),
      is_proxied_(is_proxied) {
  CHECK(!issuer_.opaque());
}

PrivateVerificationTokensToken::PrivateVerificationTokensToken(
    const PrivateVerificationTokensToken&) = default;

PrivateVerificationTokensToken& PrivateVerificationTokensToken::operator=(
    const PrivateVerificationTokensToken&) = default;

PrivateVerificationTokensToken::PrivateVerificationTokensToken(
    PrivateVerificationTokensToken&&) = default;

PrivateVerificationTokensToken& PrivateVerificationTokensToken::operator=(
    PrivateVerificationTokensToken&&) = default;

PrivateVerificationTokensToken::~PrivateVerificationTokensToken() = default;

const url::Origin& PrivateVerificationTokensToken::issuer() const {
  return issuer_;
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

base::Time PrivateVerificationTokensToken::creation_time() const {
  return creation_time_;
}

bool PrivateVerificationTokensToken::is_proxied() const {
  return is_proxied_;
}

}  // namespace private_verification_tokens
