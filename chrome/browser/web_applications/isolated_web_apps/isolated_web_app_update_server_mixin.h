// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_SERVER_MIXIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_SERVER_MIXIN_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/webapps/common/web_app_id.h"

class InProcessBrowserTest;
class Profile;

namespace web_package {
class SignedWebBundleId;
}  // namespace web_package

namespace web_app {

class BundledIsolatedWebApp;

// This mixin starts a server that hosts an update manifest and a hardcoded
// signed web bundle.
class IsolatedWebAppUpdateServerMixin : public InProcessBrowserTestMixin {
 public:
  static constexpr std::string_view kUpdateManifestFileName =
      "update_manifest.json";
  static constexpr std::string_view kBundleFileName = "bundle.swbn";

  IsolatedWebAppUpdateServerMixin(InProcessBrowserTestMixinHost* mixin_host,
                                  InProcessBrowserTest* test_base);
  ~IsolatedWebAppUpdateServerMixin() override;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  url::Origin GetAppOrigin() const;
  webapps::AppId GetAppId() const;
  web_package::SignedWebBundleId GetWebBundleId() const;
  GURL GetUpdateManifestUrl() const;

 private:
  void SetUpFilesAndServer();

  Profile* profile();

  raw_ptr<InProcessBrowserTest> test_base_;
  std::optional<IsolatedWebAppUrlInfo> url_info_;
  base::FilePath temp_dir_;
  net::EmbeddedTestServer iwa_server_;
  std::unique_ptr<BundledIsolatedWebApp> bundle_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_SERVER_MIXIN_H_
