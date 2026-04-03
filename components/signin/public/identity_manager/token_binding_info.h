// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TOKEN_BINDING_INFO_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TOKEN_BINDING_INFO_H_

#include <cstdint>
#include <vector>

namespace signin {

// Contains information required for cryptographic token binding.
// This struct is used when adding or updating accounts to pass token binding
// parameters. It is designed as a struct to be easily extensible if additional
// token binding metadata is needed in the future.
struct TokenBindingInfo {
  // A default `TokenBindingInfo` indicates that the token is unbound.
  TokenBindingInfo();

  TokenBindingInfo(const TokenBindingInfo&);
  TokenBindingInfo& operator=(const TokenBindingInfo&);
  TokenBindingInfo(TokenBindingInfo&&);
  TokenBindingInfo& operator=(TokenBindingInfo&&);

  explicit TokenBindingInfo(std::vector<uint8_t> wrapped_binding_key,
                            bool mtls_token_binding);

  ~TokenBindingInfo();

  // The cryptographic key used to bind the token, in a wrapped format.
  // An empty vector indicates that no token binding key is provided.
  std::vector<uint8_t> wrapped_binding_key;

  // Whether the refresh token is bound to an mTLS certificate. If true,
  // access token requests using this token should use mTLS-specific
  // endpoints.
  bool mtls_token_binding = false;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TOKEN_BINDING_INFO_H_
