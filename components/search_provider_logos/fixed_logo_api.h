// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_FIXED_LOGO_API_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_FIXED_LOGO_API_H_

#include <memory>
#include <string>

#include "base/time/time.h"

class GURL;

namespace search_provider_logos {

struct EncodedLogo;

// Implements AppendQueryparamsToLogoURL, defined in logo_common.h,
// for static logos.
GURL UseFixedLogoUrl(const GURL& logo_url, const std::string& fingerprint);

// Implements ParseLogoResponse, defined in logo_common.h,
// for static logos.
std::unique_ptr<EncodedLogo> ParseFixedLogoResponse(
    std::unique_ptr<std::string> response,
    base::Time response_time,
    bool* parsing_failed);

}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_FIXED_LOGO_API_H_
