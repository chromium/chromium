// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/dm_auth.h"

#include "base/memory/ptr_util.h"

namespace policy {

DMAuth::DMAuth() = default;
DMAuth::~DMAuth() = default;

DMAuth::DMAuth(DMAuth&& other) = default;
DMAuth& DMAuth::operator=(DMAuth&& other) = default;

// static
DMAuth DMAuth::FromDMToken(const std::string& dm_token) {
  return DMAuth(dm_token, DMAuthTokenType::kDm);
}

// static
DMAuth DMAuth::FromOAuthToken(const std::string& oauth_token) {
  return DMAuth(oauth_token, DMAuthTokenType::kOauth);
}

// static
DMAuth DMAuth::FromEnrollmentToken(const std::string& enrollment_token) {
  return DMAuth(enrollment_token, DMAuthTokenType::kEnrollment);
}

// static
DMAuth DMAuth::FromOidcResponse(const std::string& oidc_id_token) {
  return DMAuth(oidc_id_token, DMAuthTokenType::kOidc);
}

// static
DMAuth DMAuth::NoAuth() {
  return {};
}

DMAuth::DMAuth(const std::string& token, DMAuthTokenType token_type)
    : token_(token), token_type_(token_type) {}

bool DMAuth::operator==(const DMAuth& other) const {
  return token_ == other.token_ && token_type_ == other.token_type_;
}

bool DMAuth::operator!=(const DMAuth& other) const {
  return !(*this == other);
}

DMAuth DMAuth::Clone() const {
  return DMAuth(token_, token_type_);
}

}  // namespace policy
