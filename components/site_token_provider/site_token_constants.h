// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_CONSTANTS_H_
#define COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_CONSTANTS_H_

#include <cstddef>

namespace site_token_provider {

// The name of the injected HTTP header containing the site token.
inline constexpr char kChromeSiteTokenHeader[] =
    "CHROME-EXPERIMENTAL-SITE-TOKEN-PROVIDER";

// Maximum allowed size (in bytes) for a site token response payload (64 KB).
inline constexpr size_t kMaxTokenPayloadSize = 64 * 1024;

}  // namespace site_token_provider

#endif  // COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_CONSTANTS_H_
