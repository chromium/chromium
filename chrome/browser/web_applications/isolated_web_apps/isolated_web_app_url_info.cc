// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"

namespace web_app {

// static
base::expected<IsolatedWebAppUrlInfo, std::string>
IsolatedWebAppUrlInfo::Create(const GURL& url) {
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

  auto web_bundle_id = web_package::SignedWebBundleId::Create(url.host());
  if (!web_bundle_id.has_value()) {
    return base::unexpected(
        base::StringPrintf("The host of isolated-app:// URLs must be a valid "
                           "Signed Web Bundle ID (got %s): %s",
                           url.host().c_str(), web_bundle_id.error().c_str()));
  }

  return IsolatedWebAppUrlInfo(*web_bundle_id);
}

// static
IsolatedWebAppUrlInfo IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
    const web_package::SignedWebBundleId& web_bundle_id) {
  return IsolatedWebAppUrlInfo(web_bundle_id);
}

IsolatedWebAppUrlInfo::IsolatedWebAppUrlInfo(
    const web_package::SignedWebBundleId& web_bundle_id)
    : origin_(url::Origin::CreateFromNormalizedTuple(chrome::kIsolatedAppScheme,
                                                     web_bundle_id.id(),
                                                     /*port=*/0)),
      // The manifest id of Isolated Web Apps must resolve to the app's origin.
      // The manifest parser will resolve "id" relative the origin of the app's
      // start_url, and then sets Manifest::id to the path of this resolved URL,
      // not including a leading slash. Because of this, the resolved manifest
      // id will always be empty string.
      app_id_(GenerateAppId(/*manifest_id=*/"", origin_.GetURL())),
      web_bundle_id_(web_bundle_id) {}

const url::Origin& IsolatedWebAppUrlInfo::origin() const {
  return origin_;
}

const AppId& IsolatedWebAppUrlInfo::app_id() const {
  return app_id_;
}

const web_package::SignedWebBundleId& IsolatedWebAppUrlInfo::web_bundle_id()
    const {
  return web_bundle_id_;
}

content::StoragePartitionConfig IsolatedWebAppUrlInfo::storage_partition_config(
    content::BrowserContext* browser_context) const {
  DCHECK(browser_context != nullptr);

  constexpr char kIsolatedWebAppPartitionPrefix[] = "iwa-";
  // We add a prefix to `partition_domain` to avoid potential name conflicts
  // with Chrome Apps, which use their id/hostname as `partition_domain`.
  return content::StoragePartitionConfig::Create(
      browser_context,
      /*partition_domain=*/kIsolatedWebAppPartitionPrefix + origin().host(),
      /*partition_name=*/"",
      /*in_memory=*/false);
}

}  // namespace web_app
