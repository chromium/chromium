// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PHOSPHOR_DATA_TYPES_H_
#define COMPONENTS_PRIVATE_AI_PHOSPHOR_DATA_TYPES_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace private_ai::phosphor {

// The result of a fetch of tokens from the auth token server.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GetAuthnTokensResult)
enum class GetAuthnTokensResult {
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

  kMaxValue = kFailedOAuthTokenPersistent,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrivateAiPhosphorGetAuthnTokensResult)

// A blind-signed auth token for PrivateAI proxies.
struct BlindSignedAuthToken {
  // The token value.
  std::string token;

  // The token's encoded extensions.
  std::string encoded_extensions;

  // The expiration time of this token.
  base::Time expiration;

  bool operator==(const BlindSignedAuthToken& token) const = default;
};

}  // namespace private_ai::phosphor

#endif  // COMPONENTS_PRIVATE_AI_PHOSPHOR_DATA_TYPES_H_
