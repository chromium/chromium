// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PHOSPHOR_TOKEN_MANAGER_H_
#define COMPONENTS_PRIVATE_AI_PHOSPHOR_TOKEN_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/private_ai/phosphor/data_types.h"
#include "components/private_ai/proto/private_ai.pb.h"

namespace private_ai::phosphor {

// Manages the cache of blind-signed auth tokens for PrivateAI.
class TokenManager {
 public:
  using GetAuthTokenCallback =
      base::OnceCallback<void(std::optional<BlindSignedAuthToken>)>;

  virtual ~TokenManager() = default;

  // Gets a token for the terminal layer asynchronously.
  virtual void GetAuthToken(GetAuthTokenCallback callback) = 0;

  // Ensures that tokens are available for the terminal layer, fetching them if
  // necessary. This method is intended for pre-fetching and does not return a
  // token.
  virtual void PrefetchAuthTokens() = 0;

  // Gets a token for the proxy layer asynchronously.
  virtual void GetAuthTokenForProxy(GetAuthTokenCallback callback) = 0;

  // Ensures that tokens are available for the proxy layer, fetching them if
  // necessary. This method is intended for pre-fetching and does not return a
  // token.
  virtual void PrefetchAuthTokensForProxy() = 0;
};

}  // namespace private_ai::phosphor

#endif  // COMPONENTS_PRIVATE_AI_PHOSPHOR_TOKEN_MANAGER_H_
