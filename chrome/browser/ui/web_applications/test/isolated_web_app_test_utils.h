// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class GURL;
class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace net::test_server {
class EmbeddedTestServer;
}  // namespace net::test_server

namespace url {
class Origin;
}  // namespace url

namespace web_app {

class IsolatedWebAppUrlInfo;

inline constexpr uint8_t kTestPublicKey[] = {
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51,
    0x66, 0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02,
    0x72, 0xFE, 0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

inline constexpr uint8_t kTestPrivateKey[] = {
    0x1F, 0x27, 0x3F, 0x93, 0xE9, 0x59, 0x4E, 0xC7, 0x88, 0x82, 0xC7, 0x49,
    0xF8, 0x79, 0x3D, 0x8C, 0xDB, 0xE4, 0x60, 0x1C, 0x21, 0xF1, 0xD9, 0xF9,
    0xBC, 0x3A, 0xB5, 0xC7, 0x7F, 0x2D, 0x95, 0xE1,
    // public key (part of the private key)
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51, 0x66,
    0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02, 0x72, 0xFE,
    0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

// Derived from `kTestPublicKey`.
inline constexpr base::StringPiece kTestEd25519WebBundleId =
    "4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic";

class IsolatedWebAppBrowserTestHarness : public WebAppControllerBrowserTest {
 public:
  IsolatedWebAppBrowserTestHarness();
  IsolatedWebAppBrowserTestHarness(const IsolatedWebAppBrowserTestHarness&) =
      delete;
  IsolatedWebAppBrowserTestHarness& operator=(
      const IsolatedWebAppBrowserTestHarness&) = delete;
  ~IsolatedWebAppBrowserTestHarness() override;

 protected:
  std::unique_ptr<net::EmbeddedTestServer> CreateAndStartServer(
      const base::FilePath::StringPieceType& chrome_test_data_relative_root);
  IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
      const url::Origin& origin);
  content::RenderFrameHost* OpenApp(const AppId& app_id);
  content::RenderFrameHost* NavigateToURLInNewTab(
      Browser* window,
      const GURL& url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB);

  Browser* GetBrowserFromFrame(content::RenderFrameHost* frame);
};

std::unique_ptr<net::EmbeddedTestServer> CreateAndStartDevServer(
    const base::FilePath::StringPieceType& chrome_test_data_relative_root);

IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
    Profile* profile,
    const url::Origin& proxy_origin);

content::RenderFrameHost* OpenIsolatedWebApp(Profile* profile,
                                             const AppId& app_id);

void CreateIframe(content::RenderFrameHost* parent_frame,
                  const std::string& iframe_id,
                  const GURL& url,
                  const std::string& permissions_policy);

struct TestSignedWebBundle {
  TestSignedWebBundle(std::vector<uint8_t> data,
                      const web_package::SignedWebBundleId& id);

  TestSignedWebBundle(const TestSignedWebBundle&);
  TestSignedWebBundle(TestSignedWebBundle&&);

  ~TestSignedWebBundle();

  std::vector<uint8_t> data;
  web_package::SignedWebBundleId id;
};

class TestSignedWebBundleBuilder {
 public:
  explicit TestSignedWebBundleBuilder(
      web_package::WebBundleSigner::KeyPair key_pair =
          web_package::WebBundleSigner::KeyPair::CreateRandom());

  // Adds a manifest type payload to the bundle.
  void AddManifest(base::StringPiece manifest_string);

  // Adds a image/PNG type payload to the bundle.
  void AddPngImage(base::StringPiece url, base::StringPiece image_string);

  TestSignedWebBundle Build();

 private:
  web_package::WebBundleSigner::KeyPair key_pair_;
  web_package::WebBundleBuilder builder_;
};

TestSignedWebBundle BuildDefaultTestSignedWebBundle();

// Adds an Isolated Web App to the WebAppRegistrar. The IWA will have an empty
// filepath for |IsolatedWebAppLocation|.
AppId AddDummyIsolatedAppToRegistry(Profile* profile,
                                    const GURL& start_url,
                                    const std::string& name);
}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_
