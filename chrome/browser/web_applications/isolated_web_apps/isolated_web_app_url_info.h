// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_INFO_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class StoragePartitionConfig;
}  // namespace content

namespace web_app {

struct IsolationData;

// Wraps an Isolated Web App URL and provides methods to access data derived
// from the URL.
class IsolatedWebAppUrlInfo {
 public:
  // Creates an IsolatedWebAppUrlInfo instance from the given URL, or an error
  // message if the URL isn't valid.
  //
  // Note that this only performs basic URL validation; a non-error value does
  // not guarantee that it corresponds to an existing or installed app.
  static base::expected<IsolatedWebAppUrlInfo, std::string> Create(
      const GURL& url);

  // Creates an IsolatedWebAppUrlInfo instance from a SignedWebBundleId object.
  static IsolatedWebAppUrlInfo CreateFromSignedWebBundleId(
      const web_package::SignedWebBundleId& web_bundle_id);

  // Creates an IsolatedWebAppUrlInfo instance corresponding to the IWA
  // located at |isolation_data|.
  //
  // For proxy-based dev mode IWAs a random hostname will be generated, and
  // for signed bundles the hostname will be extracted from the bundle's
  // integrity block.
  static void CreateFromIsolationData(
      const IsolationData& isolation_data,
      base::OnceCallback<
          void(base::expected<IsolatedWebAppUrlInfo, std::string>)> callback);

  // Returns the origin of the IWA that this URL refers to.
  const url::Origin& origin() const;

  // Returns the AppId that should be used when installing the app hosted at
  // this URL.
  const AppId& app_id() const;

  // Returns the Web Bundle ID of the IWA that this URL refers to.
  const web_package::SignedWebBundleId& web_bundle_id() const;

  // Returns the StoragePartitionConfig that should be used by the resource
  // hosted at this URL.
  content::StoragePartitionConfig storage_partition_config(
      content::BrowserContext* browser_context) const;

 private:
  explicit IsolatedWebAppUrlInfo(
      const web_package::SignedWebBundleId& web_bundle_id);

  url::Origin origin_;
  AppId app_id_;
  web_package::SignedWebBundleId web_bundle_id_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_URL_INFO_H_
