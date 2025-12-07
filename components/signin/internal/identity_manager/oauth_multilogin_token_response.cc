// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_token_response.h"

namespace signin {

OAuthMultiloginTokenResponse::OAuthMultiloginTokenResponse(
    std::string oauth_token,
    std::string token_binding_assertion)
    : oauth_token_(std::move(oauth_token)),
      token_binding_assertion_(std::move(token_binding_assertion)) {}

OAuthMultiloginTokenResponse::OAuthMultiloginTokenResponse(
    OAuthMultiloginTokenResponse&& other) noexcept = default;
OAuthMultiloginTokenResponse& OAuthMultiloginTokenResponse::operator=(
    OAuthMultiloginTokenResponse&& other) noexcept = default;

OAuthMultiloginTokenResponse::~OAuthMultiloginTokenResponse() = default;

}  // namespace signin
