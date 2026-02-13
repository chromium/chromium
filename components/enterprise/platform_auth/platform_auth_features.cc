// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/platform_auth/platform_auth_features.h"

namespace enterprise_auth {

// Enables native SSO support with Okta services.
BASE_FEATURE(kOktaSSO, base::FEATURE_DISABLED_BY_DEFAULT);

// Allowlist for request headers on the Okta SSO URL request.
// Header names must be lowercase. The list is comma-separated.
// If this list is empty all request headers will be allowed.
BASE_FEATURE_PARAM(std::string,
                   kOktaSsoRequestHeadersAllowlist,
                   &kOktaSSO,
                   "OktaSsoRequestHeadersAllowlist",
                   "accept,accept-language,content-type,user-agent,x-okta-user-"
                   "agent-extended");

// Fixed request headers appended to the Okta SSO URL request.
// Format: list of pipe-separated pairs. Values within a pair are
// semicolon-separated.
BASE_FEATURE_PARAM(
    std::string,
    kOktaSsoFixedRequestHeaders,
    &kOktaSSO,
    "kOktaSsoFixedRequestHeaders",
    "Cache-Control;no-cache|Pragma;no-cache|Priority;u=1, "
    "i|Sec-Fetch-Dest;empty|Sec-Fetch-Mode;cors|Sec-Fetch-Site;same-origin");

// The pattern for a SSO URL request path specific to the Okta IdP.
// Format: segments separated with |/|. * is a wildcard matching 1 segment.
BASE_FEATURE_PARAM(
    std::string,
    kOktaSsoURLPattern,
    &kOktaSSO,
    "OktaSsoURLPattern",
    "/idp/idx/authenticators/sso_extension/transactions/*/verify");

}  // namespace enterprise_auth
