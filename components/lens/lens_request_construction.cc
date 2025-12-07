// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_request_construction.h"

#include "base/strings/stringprintf.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"

namespace lens {
constexpr char kClientDataHeader[] = "X-Client-Data";
constexpr char kDeveloperKey[] = "X-Developer-Key";

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

std::vector<std::string> CreateVariationsHeaders(
    variations::VariationsClient* variations_client) {
  std::vector<std::string> headers;
  variations::mojom::VariationsHeadersPtr variations =
      variations_client->GetVariationsHeaders();
  if (variations_client->IsOffTheRecord() || variations.is_null()) {
    return headers;
  }

  headers.push_back(kClientDataHeader);
  // The endpoint is always a Google property.
  headers.push_back(variations->headers_map.at(
      variations::mojom::GoogleWebVisibility::FIRST_PARTY));

  return headers;
}

}  // namespace lens
