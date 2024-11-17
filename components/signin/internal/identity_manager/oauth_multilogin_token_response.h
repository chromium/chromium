// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_RESPONSE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_RESPONSE_H_

#include <string>

#include "components/signin/public/base/signin_buildflags.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include <optional>

#include "components/signin/public/base/hybrid_encryption_key.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

namespace signin {

// Class holding a success return value of a multilogin token request for a
// single account.
class OAuthMultiloginTokenResponse {
 public:
  explicit OAuthMultiloginTokenResponse(
      std::string oauth_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      std::string token_binding_assertion = std::string(),
      std::optional<HybridEncryptionKey> ephemeral_key = std::nullopt
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  );

  // Move-able but not copy-able.
  OAuthMultiloginTokenResponse(const OAuthMultiloginTokenResponse&) = delete;
  OAuthMultiloginTokenResponse& operator=(const OAuthMultiloginTokenResponse&) =
      delete;
  OAuthMultiloginTokenResponse(OAuthMultiloginTokenResponse&& other) noexcept;
  OAuthMultiloginTokenResponse& operator=(
      OAuthMultiloginTokenResponse&& other) noexcept;

  ~OAuthMultiloginTokenResponse();

  // Access or refresh OAuth token.
  const std::string& oauth_token() const { return oauth_token_; }

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  // An assertion proving possession of a private key if `oauth_token` is bound.
  // May be empty.
  const std::string& token_binding_assertion() const {
    return token_binding_assertion_;
  }

  // If `token_binding_assertion` is not empty and uses the hybrid encryption,
  // this key can be used to decrypt cookies from the server response.
  const std::optional<HybridEncryptionKey>& ephemeral_key() const {
    return ephemeral_key_;
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

 private:
  std::string oauth_token_;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  std::string token_binding_assertion_;
  std::optional<HybridEncryptionKey> ephemeral_key_;
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_RESPONSE_H_
