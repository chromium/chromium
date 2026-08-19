// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_token_provider/features.h"

namespace site_token_provider::features {

// Enables the registration and creation of the SiteTokenProvider component.
BASE_FEATURE(kSiteTokenProviderEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

// The OAuth2 scope used to request site tokens. Configured via Finch.
const base::FeatureParam<std::string> kSiteTokenOAuth2Scope{
    &kSiteTokenProviderEnabled, "oauth2_scope", ""};

// The URL endpoint used to retrieve site tokens. Configured via Finch.
const base::FeatureParam<std::string> kSiteTokenEndpointUrl{
    &kSiteTokenProviderEnabled, "site_token_endpoint_url", ""};

}  // namespace site_token_provider::features
