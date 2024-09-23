// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_SERVER_MIXIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_SERVER_MIXIN_H_

#include <memory>
#include <optional>

#include "base/version.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

class InProcessBrowserTest;

namespace web_app {

class BundledIsolatedWebApp;

// This mixin starts a server that hosts update manifests and bundles.
class IsolatedWebAppUpdateServerMixin : public InProcessBrowserTestMixin {
 public:
  explicit IsolatedWebAppUpdateServerMixin(
      InProcessBrowserTestMixinHost* mixin_host);
  ~IsolatedWebAppUpdateServerMixin() override;

  // Sets up `iwa_server_`.
  void SetUpOnMainThread() override;

  // The returned URL has the following structure:
  //   * /<web_bundle_id>/update_manifest.json
  GURL GetUpdateManifestUrl(
      const web_package::SignedWebBundleId& web_bundle_id) const;

#if BUILDFLAG(IS_CHROMEOS)
  // Generates a policy entry that can be appended to
  // `prefs::kIsolatedWebAppInstallForceList` in order to force-install the IWA.
  base::Value::Dict CreateForceInstallPolicyEntry(
      const web_package::SignedWebBundleId& web_bundle_id) const;
#endif

  // Adds a bundle to the update server and starts tracking it in the
  // corresponding update manifest.
  void AddBundle(std::unique_ptr<BundledIsolatedWebApp> bundle);

  // Removes the bundle with `version` for `web_bundle_id` and stops tracking it
  // in the corresponding update manifest. Will CHECK if this bundle is not
  // currently served.
  void RemoveBundle(const web_package::SignedWebBundleId& web_bundle_id,
                    const base::Version& version);

 private:
  // Handles the following routes:
  //  * /<web_bundle_id>/update_manifest.json
  //  * /<web_bundle_id>/<version>.swbn
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  net::EmbeddedTestServer iwa_server_;
  base::flat_map<
      web_package::SignedWebBundleId,
      base::flat_map<base::Version, std::unique_ptr<BundledIsolatedWebApp>>>
      bundle_versions_per_id_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_SERVER_MIXIN_H_
