// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_RESPONSE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_RESPONSE_H_

#include <string>

#include "components/signin/public/base/signin_buildflags.h"

namespace signin {

// Class holding a success return value of a multilogin token request for a
// single account.
class OAuthMultiloginTokenResponse {
 public:
  explicit OAuthMultiloginTokenResponse(
      std::string oauth_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      std::string token_binding_assertion = std::string()
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
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

 private:
  std::string oauth_token_;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  std::string token_binding_assertion_;
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_RESPONSE_H_
