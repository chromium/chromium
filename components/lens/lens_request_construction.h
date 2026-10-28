// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_REQUEST_CONSTRUCTION_H_
#define COMPONENTS_LENS_LENS_REQUEST_CONSTRUCTION_H_

#include <string>
#include <vector>

namespace endpoint_fetcher {
struct EndpointResponse;
}  // namespace endpoint_fetcher

namespace signin {
struct AccessTokenInfo;
}  // namespace signin

namespace variations {
class VariationsClient;
}  // namespace variations

class GoogleServiceAuthError;

namespace lens {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensIdentityDelegationRetryRequestType)
enum class LensIdentityDelegationRetryRequestType {
  kComposeboxClusterInfo = 0,
  kComposeboxFetchRequest = 1,
  kOverlayClusterInfo = 2,
  kOverlayFetchRequest = 3,
  kMaxValue = kOverlayFetchRequest,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensIdentityDelegationRetryRequestType)

// Creates the OAuth headers for Lens requests.
std::vector<std::string> CreateOAuthHeader(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info);

// Creates the variations headers for Lens requests.
std::vector<std::string> CreateVariationsHeaders(
    variations::VariationsClient* variations_client);

// Removes the "Authorization" header (and its value) from `headers`, which is
// formatted as alternating key-value strings. Returns true if an
// "Authorization" header was present and removed.
bool StripAuthorizationHeader(std::vector<std::string>& headers);

// Returns true, strips the "Authorization" header from `headers`, and records
// `request_type` to `Lens.IdentityDelegation.RetriedWithoutAuthOn401` if
// identity delegation is enabled, `response` failed with HTTP 401
// (HTTP_UNAUTHORIZED), and `headers` contained an "Authorization" header.
// `response` may be null, in which case this returns false.
bool ShouldRetryRequestWithoutAuthOn401(
    const endpoint_fetcher::EndpointResponse* response,
    std::vector<std::string>& headers,
    LensIdentityDelegationRetryRequestType request_type);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_REQUEST_CONSTRUCTION_H_
