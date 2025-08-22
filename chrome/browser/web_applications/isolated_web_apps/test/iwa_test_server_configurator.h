// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_IWA_TEST_SERVER_CONFIGURATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_IWA_TEST_SERVER_CONFIGURATOR_H_

#include <vector>

#include "chrome/browser/web_applications/isolated_web_apps/test/bundle_versions_storage.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "url/gurl.h"

namespace network {
class TestURLLoaderFactory;
}

namespace web_app {
// Configures IWA self hosted server for unit tests.
class IwaTestServerConfigurator {
 public:
  explicit IwaTestServerConfigurator(network::TestURLLoaderFactory& factory);
  ~IwaTestServerConfigurator();

  // Adds a bundle to be served to `factory_` at a well-known url and updates
  // the manifest served for this bundle's id.
  void AddBundle(
      std::unique_ptr<BundledIsolatedWebApp> bundle,
      std::optional<std::vector<UpdateChannel>> update_channels = std::nullopt);

  void SetServedUpdateManifestResponse(
      const web_package::SignedWebBundleId& web_bundle_id,
      net::HttpStatusCode http_status,
      std::string_view json_content);

  // Generates a policy entry that can be appended to
  // `prefs::kIsolatedWebAppInstallForceList` in order to force-install the IWA.
  // Delegates to `test::CreateForceInstallIwaPolicyEntry()` with a custom
  // `update_manifest_url` that `factory_` can process.
  static base::Value::Dict CreateForceInstallPolicyEntry(
      const web_package::SignedWebBundleId& web_bundle_id,
      const std::optional<UpdateChannel>& update_channel = std::nullopt,
      const std::optional<IwaVersion>& pinned_version = std::nullopt,
      bool allow_downgrades = false);

  GURL GetUpdateManifestUrlForIwa(
      const web_package::SignedWebBundleId& web_bundle_id) {
    return storage_.GetUpdateManifestUrl(web_bundle_id);
  }

 private:
  test::BundleVersionsStorage storage_;

  const raw_ref<network::TestURLLoaderFactory> factory_;
};
}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_IWA_TEST_SERVER_CONFIGURATOR_H_
