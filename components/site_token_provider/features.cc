// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_token_provider/features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace site_token_provider::features {

// Enables the registration and creation of the SiteTokenProvider component.
BASE_FEATURE(kSiteTokenProviderEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

// The OAuth2 scope used to request site tokens. Configured via Finch.
constexpr base::FeatureParam<std::string> kSiteTokenOAuth2Scope{
    &kSiteTokenProviderEnabled, "oauth2_scope", ""};

// The URL endpoint used to retrieve site tokens. Configured via Finch.
constexpr base::FeatureParam<std::string> kSiteTokenEndpointUrl{
    &kSiteTokenProviderEnabled, "site_token_endpoint_url", ""};

// List of domains that are allowed to use site tokens. Configured via Finch.
constexpr base::FeatureParam<std::string> kSiteTokenAllowlist{
    &kSiteTokenProviderEnabled, "site_token_allowlist", ""};

// Background token refresh interval. A non-positive value disables it.
constexpr base::FeatureParam<base::TimeDelta> kSiteTokenRefreshInterval{
    &kSiteTokenProviderEnabled, "site_token_refresh_interval", base::Days(14)};

}  // namespace site_token_provider::features
