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
  inline static constexpr uint8_t kTestPublicKey[] = {
      0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51,
      0x66, 0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02,
      0x72, 0xFE, 0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};
  inline static constexpr uint8_t kTestPrivateKey[] = {
      0x1F, 0x27, 0x3F, 0x93, 0xE9, 0x59, 0x4E, 0xC7, 0x88, 0x82, 0xC7, 0x49,
      0xF8, 0x79, 0x3D, 0x8C, 0xDB, 0xE4, 0x60, 0x1C, 0x21, 0xF1, 0xD9, 0xF9,
      0xBC, 0x3A, 0xB5, 0xC7, 0x7F, 0x2D, 0x95, 0xE1,
      // public key (part of the private key)
      0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51, 0x66,
      0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02, 0x72, 0xFE,
      0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};
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
  web_package::WebBundleSigner::Ed25519KeyPair key_pair_ =
      web_package::WebBundleSigner::Ed25519KeyPair(kTestPublicKey,
                                                   kTestPrivateKey);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_SERVER_MIXIN_H_
