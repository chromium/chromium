// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_token_response.h"

namespace signin {

OAuthMultiloginTokenResponse::OAuthMultiloginTokenResponse(
    std::string oauth_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    ,
    std::string token_binding_assertion,
    std::optional<HybridEncryptionKey> ephemeral_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    )
    : oauth_token_(std::move(oauth_token))
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      token_binding_assertion_(std::move(token_binding_assertion)),
      ephemeral_key_(std::move(ephemeral_key))
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
{
}

OAuthMultiloginTokenResponse::OAuthMultiloginTokenResponse(
    OAuthMultiloginTokenResponse&& other) noexcept = default;
OAuthMultiloginTokenResponse& OAuthMultiloginTokenResponse::operator=(
    OAuthMultiloginTokenResponse&& other) noexcept = default;

OAuthMultiloginTokenResponse::~OAuthMultiloginTokenResponse() = default;

}  // namespace signin
