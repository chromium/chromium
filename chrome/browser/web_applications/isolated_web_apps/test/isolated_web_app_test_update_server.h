// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_TEST_UPDATE_SERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_TEST_UPDATE_SERVER_H_

#include <memory>
#include <optional>
#include <vector>

#include "chrome/browser/web_applications/isolated_web_apps/test/bundle_versions_storage.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace web_app {

class BundledIsolatedWebApp;

// This mixin starts a server that hosts update manifests and bundles.
class IsolatedWebAppTestUpdateServer {
 public:
  IsolatedWebAppTestUpdateServer();
  ~IsolatedWebAppTestUpdateServer();

  IsolatedWebAppTestUpdateServer(const IsolatedWebAppTestUpdateServer&) =
      delete;
  IsolatedWebAppTestUpdateServer& operator=(
      const IsolatedWebAppTestUpdateServer&) = delete;

  // The returned URL has the following structure:
  //   * /<web_bundle_id>/update_manifest.json
  GURL GetUpdateManifestUrl(
      const web_package::SignedWebBundleId& web_bundle_id) const;

  // Generates a policy entry that can be appended to
  // `prefs::kIsolatedWebAppInstallForceList` in order to force-install the IWA.
  base::Value::Dict CreateForceInstallPolicyEntry(
      const web_package::SignedWebBundleId& web_bundle_id,
      const std::optional<UpdateChannel>& update_channel = std::nullopt,
      const std::optional<IwaVersion>& pinned_version = std::nullopt,
      const bool allow_downgrades = false) const;

  // Returns the update manifest for `web_bundle_id`. Will CHECK if there are no
  // bundles served for this `web_bundle_id`.
  base::Value::Dict GetUpdateManifest(
      const web_package::SignedWebBundleId& web_bundle_id) const;

  // Adds a bundle to the update server and starts tracking it in the
  // corresponding update manifest.
  void AddBundle(
      std::unique_ptr<BundledIsolatedWebApp> bundle,
      std::optional<std::vector<UpdateChannel>> update_channels = std::nullopt);

  // Removes the bundle with `version` for `web_bundle_id` and stops tracking it
  // in the corresponding update manifest. Will CHECK if this bundle is not
  // currently served.
  void RemoveBundle(const web_package::SignedWebBundleId& web_bundle_id,
                    const IwaVersion& version);

 private:
  // Handles the following routes:
  //  * /<web_bundle_id>/update_manifest.json
  //  * /<web_bundle_id>/<version>.swbn
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  net::EmbeddedTestServer iwa_server_;
  test::BundleVersionsStorage storage_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_TEST_UPDATE_SERVER_H_
