// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_utils.h"

#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "url/gurl.h"

namespace web_app {

base::expected<web_package::SignedWebBundleId, std::string> ParseIsolatedAppUrl(
    const GURL& url) {
  if (!url.is_valid()) {
    return base::unexpected("Invalid URL");
  }
  if (!url.SchemeIs(chrome::kIsolatedAppScheme)) {
    return base::unexpected(
        base::StringPrintf("The URL scheme must be %s, but was %s",
                           chrome::kIsolatedAppScheme, url.scheme().c_str()));
  }

  // Valid isolated-app:// `GURL`s can never include credentials or ports, since
  // the scheme is configured as `url::SCHEME_WITH_HOST`. The `DCHECK` is here
  // just in case, but should never trigger as long as the scheme is configured
  // correctly.
  DCHECK(!url.has_username() && !url.has_password() && !url.has_port() &&
         url.IsStandard());

  auto web_bundle_id = web_package::SignedWebBundleId::Create(url.host_piece());
  if (!web_bundle_id.has_value()) {
    return base::unexpected(
        base::StringPrintf("The host of isolated-app:// URLs must be a valid "
                           "Signed Web Bundle ID (got %s): %s",
                           url.host().c_str(), web_bundle_id.error().c_str()));
  }

  return *web_bundle_id;
}

}  // namespace web_app
