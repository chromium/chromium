// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_REQUEST_CONSTRUCTION_H_
#define COMPONENTS_LENS_LENS_REQUEST_CONSTRUCTION_H_

#include <string>
#include <vector>

namespace signin {
struct AccessTokenInfo;
}  // namespace signin

namespace variations {
class VariationsClient;
}  // namespace variations

class GoogleServiceAuthError;

namespace lens {

// Creates the OAuth headers for Lens requests.
std::vector<std::string> CreateOAuthHeader(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info);

// Creates the variations headers for Lens requests.
std::vector<std::string> CreateVariationsHeaders(
    variations::VariationsClient* variations_client);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_REQUEST_CONSTRUCTION_H_
