// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_request_construction.h"

#include "base/strings/stringprintf.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"

namespace lens {
constexpr char kDeveloperKey[] = "X-Developer-Key";

// TODO(crbug.com/424869589): Clean up code duplication with
// LensOverlayQueryController.
std::vector<std::string> CreateOAuthHeader(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  std::vector<std::string> headers;
  if (error.state() == GoogleServiceAuthError::NONE) {
    headers.push_back(kDeveloperKey);
    headers.push_back(GaiaUrls::GetInstance()->oauth2_chrome_client_id());
    headers.push_back(net::HttpRequestHeaders::kAuthorization);
    headers.push_back(
        base::StringPrintf("Bearer %s", access_token_info.token.c_str()));
  }
  return headers;
}
}  // namespace lens
