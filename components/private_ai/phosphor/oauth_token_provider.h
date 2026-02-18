// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PHOSPHOR_OAUTH_TOKEN_PROVIDER_H_
#define COMPONENTS_PRIVATE_AI_PHOSPHOR_OAUTH_TOKEN_PROVIDER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/private_ai/phosphor/data_types.h"

namespace private_ai::phosphor {

// Interface for providing OAuth tokens.
class OAuthTokenProvider {
 public:
  virtual ~OAuthTokenProvider() = default;

  // Checks if token fetching is enabled.
  virtual bool IsTokenFetchEnabled() = 0;

  // Calls the IdentityManager asynchronously to request the OAuth token for
  // the logged in user, or nullopt and an error code.
  using RequestOAuthTokenCallback =
      base::OnceCallback<void(GetAuthnTokensResult result,
                              std::optional<std::string> token)>;
  virtual void RequestOAuthToken(RequestOAuthTokenCallback callback) = 0;
};

}  // namespace private_ai::phosphor

#endif  // COMPONENTS_PRIVATE_AI_PHOSPHOR_OAUTH_TOKEN_PROVIDER_H_
