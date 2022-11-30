// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_INFO_H_

#include <string>

#include "base/types/expected.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class StoragePartitionConfig;
}  // namespace content

namespace web_package {
class SignedWebBundleId;
}

namespace web_app {

// Wraps an Isolated Web App URL and provides methods to access data derived
// from the URL.
class IsolatedWebAppUrlInfo {
 public:
  // Creates an IsolatedWebAppUrlInfo instance from the given URL, or an error
  // message if the URL isn't valid.
  //
  // Note that this only performs basic URL validation; a non-error value does
  // not guarantee the URL contains a valid key in its hostname, or that it
  // corresponds to an existing or installed app.
  static base::expected<IsolatedWebAppUrlInfo, std::string> Create(
      const GURL& url);

  // Wraps Create() but accepts a SignedWebBundleId object.
  static base::expected<IsolatedWebAppUrlInfo, std::string>
  CreateFromSignedWebBundleId(
      const web_package::SignedWebBundleId& web_bundle_id);

  // Returns the origin of the IWA that this URL refers to.
  const url::Origin& origin() const;

  // Returns the AppId that should be used when installing the app hosted at
  // this URL.
  const AppId& app_id() const;

  // Returns the StoragePartitionConfig that should be used by the resource
  // hosted at this URL.
  content::StoragePartitionConfig storage_partition_config(
      content::BrowserContext* browser_context) const;

  // Parses a `SignedWebBundleId` from the URL, verifying that it is a valid
  // isolated-app:// URL. Returns an error message on failure.
  base::expected<web_package::SignedWebBundleId, std::string>
  ParseSignedWebBundleId() const;

 private:
  explicit IsolatedWebAppUrlInfo(const GURL& url);

  GURL url_;
  url::Origin origin_;
  AppId app_id_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_INFO_H_
