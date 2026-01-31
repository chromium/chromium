// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_PHOSPHOR_TOKEN_MANAGER_H_
#define COMPONENTS_LEGION_PHOSPHOR_TOKEN_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/legion/phosphor/data_types.h"
#include "components/legion/proto/legion.pb.h"

namespace legion::phosphor {

// Manages the cache of blind-signed auth tokens for Legion.
class TokenManager {
 public:
  using GetAuthTokenCallback =
      base::OnceCallback<void(std::optional<BlindSignedAuthToken>)>;

  virtual ~TokenManager() = default;

  // Gets a token for the given feature asynchronously.
  virtual void GetAuthToken(proto::FeatureName feature_name,
                            GetAuthTokenCallback callback) = 0;

  // Ensures that tokens are available for the given feature, fetching them if
  // necessary. This method is intended for pre-fetching and does not return a
  // token.
  virtual void PrefetchAuthTokens(proto::FeatureName feature_name) = 0;
};

}  // namespace legion::phosphor

#endif  // COMPONENTS_LEGION_PHOSPHOR_TOKEN_MANAGER_H_
