// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UTILS_H_

#include <string>

#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

class GURL;

namespace web_app {

// Parses a `SignedWebBundleId` from a URL, verifying that the URL is a valid
// isolated-app:// URL. Returns an error message on failure.
base::expected<web_package::SignedWebBundleId, std::string> ParseIsolatedAppUrl(
    const GURL& url);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UTILS_H_
