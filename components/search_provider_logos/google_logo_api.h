// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_GOOGLE_LOGO_API_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_GOOGLE_LOGO_API_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/search_provider_logos/logo_common.h"
#include "url/gurl.h"

namespace search_provider_logos {

// Returns the URL where the Google doodle can be downloaded, e.g.
// https://www.google.com/async/ddljson. This depends on the user's
// Google domain.
GURL GetGoogleDoodleURL(const GURL& google_base_url);

// Implements AppendQueryparamsToLogoURL, defined in logo_common.h. Using this
// relies on the caller having prepared the |logo_url| by calling
// AppendPreliminaryParamsToDoodleURL().
GURL AppendFingerprintParamToDoodleURL(const GURL& logo_url,
                                       const std::string& fingerprint);

// Builds the reference URL for the current requested Doodle preset. This URL
// will have to be processed again to add the fingerprint before making the
// request to the server, see AppendFingerprintParamToDoodleURL().
GURL AppendPreliminaryParamsToDoodleURL(bool gray_background,
                                        bool for_webui_ntp,
                                        const GURL& logo_url);

// Implements ParseLogoResponse, defined in logo_common.h, for Google or
// third-party doodles.
std::unique_ptr<EncodedLogo> ParseDoodleLogoResponse(
    const GURL& base_url,
    std::unique_ptr<std::string> response,
    base::Time response_time,
    bool* parsing_failed);

}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_GOOGLE_LOGO_API_H_
