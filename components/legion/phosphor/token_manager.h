// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_PHOSPHOR_TOKEN_MANAGER_H_
#define COMPONENTS_LEGION_PHOSPHOR_TOKEN_MANAGER_H_

#include <optional>
#include <string>

#include "components/legion/phosphor/data_types.h"
#include "components/legion/proto/legion.pb.h"

namespace legion::phosphor {

// Manages the cache of blind-signed auth tokens for Legion.
class TokenManager {
 public:
  virtual ~TokenManager() = default;

  // Checks whether tokens are available for a particular feature name.
  virtual bool IsAuthTokenAvailable(proto::FeatureName feature_name) = 0;

  // Gets a token, if one is available for the given feature.
  virtual std::optional<BlindSignedAuthToken> GetAuthToken(
      proto::FeatureName feature_name) = 0;
};

}  // namespace legion::phosphor

#endif  // COMPONENTS_LEGION_PHOSPHOR_TOKEN_MANAGER_H_
