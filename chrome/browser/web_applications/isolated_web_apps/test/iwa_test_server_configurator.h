// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_IWA_TEST_SERVER_CONFIGURATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_IWA_TEST_SERVER_CONFIGURATOR_H_

#include <vector>

#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "url/gurl.h"

namespace network {
class TestURLLoaderFactory;
}

namespace web_app {
// Configures IWA self hosted server for unit tests.
class IwaTestServerConfigurator {
 public:
  IwaTestServerConfigurator();
  ~IwaTestServerConfigurator();

  void AddUpdateManifest(std::string relative_url,
                         std::string update_manifest_value);
  void AddSignedWebBundle(std::string relative_url,
                          web_app::TestSignedWebBundle web_bundle);

  // Configures TestURLLoaderFactory and FakeWebContentsManager for unittests.
  void ConfigureURLLoader(const GURL& base_url,
                          network::TestURLLoaderFactory& test_factory,
                          FakeWebContentsManager& fake_web_contents_manager);

 private:
  struct ServedUpdateManifest {
    std::string relative_url_;
    std::string manifest_value_;
  };
  std::vector<ServedUpdateManifest> served_update_manifests_;

  struct ServedSignedWebBundle {
    std::string relative_url_;
    web_app::TestSignedWebBundle web_bundle_;
  };
  std::vector<ServedSignedWebBundle> served_signed_web_bundles_;
};
}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_IWA_TEST_SERVER_CONFIGURATOR_H_
