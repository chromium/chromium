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

namespace {
const char kIsolatedAppPartitionPrefix[] = "iwa-";
}

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

  return IsolatedWebAppUrlInfo(url);
}

// static
base::expected<IsolatedWebAppUrlInfo, std::string>
IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
    const web_package::SignedWebBundleId& web_bundle_id) {
  const GURL url =
      GURL(base::StrCat({chrome::kIsolatedAppScheme,
                         url::kStandardSchemeSeparator, web_bundle_id.id()}));
  return IsolatedWebAppUrlInfo::Create(url);
}

IsolatedWebAppUrlInfo::IsolatedWebAppUrlInfo(const GURL& url)
    : url_(url),
      origin_(url::Origin::Create(url)),
      // The manifest id of Isolated Web Apps must resolve to the app's origin.
      // The manifest parser will resolve "id" relative the origin of the app's
      // start_url, and then sets Manifest::id to the path of this resolved URL,
      // not including a leading slash. Because of this, the resolved manifest
      // id will always be empty string.
      app_id_(GenerateAppId(/*manifest_id=*/"", origin_.GetURL())) {}

const url::Origin& IsolatedWebAppUrlInfo::origin() const {
  return origin_;
}

const AppId& IsolatedWebAppUrlInfo::app_id() const {
  return app_id_;
}

content::StoragePartitionConfig IsolatedWebAppUrlInfo::storage_partition_config(
    content::BrowserContext* browser_context) const {
  // We add a prefix to `partition_domain` to avoid potential name conflicts
  // with Chrome Apps, which use their id/hostname as `partition_domain`.
  return content::StoragePartitionConfig::Create(
      browser_context,
      /*partition_domain=*/kIsolatedAppPartitionPrefix + origin().host(),
      /*partition_name=*/"",
      /*in_memory=*/false);
}

base::expected<web_package::SignedWebBundleId, std::string>
IsolatedWebAppUrlInfo::ParseSignedWebBundleId() const {
  auto web_bundle_id = web_package::SignedWebBundleId::Create(origin().host());
  if (!web_bundle_id.has_value()) {
    return base::unexpected(
        base::StringPrintf("The host of isolated-app:// URLs must be a valid "
                           "Signed Web Bundle ID (got %s): %s",
                           url_.host().c_str(), web_bundle_id.error().c_str()));
  }

  return *web_bundle_id;
}

}  // namespace web_app
