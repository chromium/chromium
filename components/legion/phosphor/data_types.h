// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_PHOSPHOR_DATA_TYPES_H_
#define COMPONENTS_LEGION_PHOSPHOR_DATA_TYPES_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace legion::phosphor {

// The result of a fetch of tokens from the auth token server.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep this in sync with
// LegionTryGetAuthTokensResult in enums.xml.
enum class TryGetAuthTokensResult {
  // The request was successful and resulted in new tokens.
  kSuccess = 0,
  // No primary account is set.
  kFailedNoAccount = 1,
  // Chrome determined the primary account is not eligible.
  kFailedNotEligible = 2,
  // There was a failure fetching an OAuth token for the primary account.
  // Deprecated in favor of `kFailedOAuthToken{Transient,Persistent}`.
  kFailedOAuthTokenDeprecated = 3,
  // There was a failure in BSA with the given status code.
  kFailedBSA400 = 4,
  kFailedBSA401 = 5,
  kFailedBSA403 = 6,

  // Any other issue calling BSA.
  kFailedBSAOther = 7,

  // There was a transient failure fetching an OAuth token for the primary
  // account.
  kFailedOAuthTokenTransient = 8,
  // There was a persistent failure fetching an OAuth token for the primary
  // account.
  kFailedOAuthTokenPersistent = 9,

  // The attempt to request tokens failed because the feature was disabled by
  // the user.
  kFailedDisabledByUser = 10,

  kMaxValue = kFailedDisabledByUser,
};

// A blind-signed auth token for Legion proxies.
struct BlindSignedAuthToken {
  // The token value, for inclusion in a header.
  std::string token;

  // The expiration time of this token.
  base::Time expiration;

  bool operator==(const BlindSignedAuthToken& token) const = default;
};

}  // namespace legion::phosphor

#endif  // COMPONENTS_LEGION_PHOSPHOR_DATA_TYPES_H_
