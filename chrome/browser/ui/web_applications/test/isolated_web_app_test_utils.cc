// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkImageEncoder.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {
namespace {
constexpr base::StringPiece kTestManifest = R"({
      "name": "Simple Isolated App",
      "id": "/",
      "scope": "/",
      "start_url": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "256x256-green.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ]
    })";

constexpr base::StringPiece kTestIconUrl = "/256x256-green.png";

std::string GetTestIconInString() {
  SkBitmap icon_bitmap = CreateSquareIcon(256, SK_ColorGREEN);
  sk_sp<SkData> icon_skdata =
      SkEncodeBitmap(icon_bitmap, SkEncodedImageFormat::kPNG, 100);
  return std::string(static_cast<const char*>(icon_skdata->data()),
                     icon_skdata->size());
}
}  // namespace

IsolatedWebAppBrowserTestHarness::IsolatedWebAppBrowserTestHarness() = default;

IsolatedWebAppBrowserTestHarness::~IsolatedWebAppBrowserTestHarness() = default;

std::unique_ptr<net::EmbeddedTestServer>
IsolatedWebAppBrowserTestHarness::CreateAndStartServer(
    const base::FilePath::StringPieceType& chrome_test_data_relative_root) {
  base::FilePath server_root =
      GetChromeTestDataDir().Append(chrome_test_data_relative_root);
  auto server = std::make_unique<net::EmbeddedTestServer>();
  server->AddDefaultHandlers(server_root);
  CHECK(server->Start());
  return server;
}

AppId IsolatedWebAppBrowserTestHarness::InstallIsolatedWebApp(
    const std::string& host) {
  GURL app_url = https_server()->GetURL(host,
                                        "/banners/manifest_test_page.html"
                                        "?manifest=manifest_isolated.json");
  return InstallIsolatedWebApp(app_url);
}

AppId IsolatedWebAppBrowserTestHarness::InstallIsolatedWebApp(
    const GURL& app_url) {
  EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), app_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  return test::InstallPwaForCurrentUrl(browser());
}

IsolatedWebAppUrlInfo
IsolatedWebAppBrowserTestHarness::InstallDevModeProxyIsolatedWebApp(
    const url::Origin& origin) {
  base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                        InstallIsolatedWebAppCommandError>>
      future;

  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      web_package::SignedWebBundleId::CreateRandomForDevelopment());
  WebAppProvider::GetForWebApps(profile())->scheduler().InstallIsolatedWebApp(
      url_info, DevModeProxy{.proxy_url = origin}, future.GetCallback());

  CHECK(future.Get().has_value()) << future.Get().error();

  return url_info;
}

Browser* IsolatedWebAppBrowserTestHarness::GetBrowserFromFrame(
    content::RenderFrameHost* frame) {
  Browser* browser = chrome::FindBrowserWithWebContents(
      content::WebContents::FromRenderFrameHost(frame));
  EXPECT_TRUE(browser);
  return browser;
}

void IsolatedWebAppBrowserTestHarness::CreateIframe(
    content::RenderFrameHost* parent_frame,
    const std::string& iframe_id,
    const GURL& url,
    const std::string& permissions_policy) {
  EXPECT_EQ(true, content::EvalJs(
                      parent_frame,
                      content::JsReplace(R"(
            new Promise(resolve => {
              let f = document.createElement('iframe');
              f.id = $1;
              f.src = $2;
              f.allow = $3;
              f.addEventListener('load', () => resolve(true));
              document.body.appendChild(f);
            });
        )",
                                         iframe_id, url, permissions_policy)));
}

content::RenderFrameHost* IsolatedWebAppBrowserTestHarness::OpenApp(
    const AppId& app_id) {
  WebAppRegistrar& registrar =
      WebAppProvider::GetForWebApps(profile())->registrar_unsafe();
  const WebApp* app = registrar.GetAppById(app_id);
  EXPECT_TRUE(app);
  Browser* app_window = Browser::Create(Browser::CreateParams::CreateForApp(
      GenerateApplicationNameFromAppId(app->app_id()),
      /*trusted_source=*/true, gfx::Rect(), profile(),
      /*user_gesture=*/true));
  return NavigateToURLInNewTab(app_window, app->start_url());
}

content::RenderFrameHost*
IsolatedWebAppBrowserTestHarness::NavigateToURLInNewTab(
    Browser* window,
    const GURL& url,
    WindowOpenDisposition disposition) {
  auto new_contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser()->profile()));
  window->tab_strip_model()->AppendWebContents(std::move(new_contents),
                                               /*foreground=*/true);
  return ui_test_utils::NavigateToURLWithDisposition(
      window, url, disposition, ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

TestSignedWebBundle::TestSignedWebBundle(
    std::vector<uint8_t> data,
    const web_package::SignedWebBundleId& id)
    : data(std::move(data)), id(id) {}

TestSignedWebBundle::TestSignedWebBundle(const TestSignedWebBundle&) = default;

TestSignedWebBundle::TestSignedWebBundle(TestSignedWebBundle&&) = default;

TestSignedWebBundle::~TestSignedWebBundle() = default;

TestSignedWebBundleBuilder::TestSignedWebBundleBuilder(
    web_package::WebBundleSigner::KeyPair key_pair)
    : key_pair_(key_pair) {}

void TestSignedWebBundleBuilder::AddManifest(
    base::StringPiece manifest_string) {
  // TODO(crbug.com/1385393): Remove base URL once relative URL is supported.
  GURL base_url = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                      web_package::SignedWebBundleId::CreateForEd25519PublicKey(
                          (key_pair_.public_key)))
                      .origin()
                      .GetURL();
  builder_.AddExchange(
      base_url.Resolve("/manifest.webmanifest"),
      {{":status", "200"}, {"content-type", "application/manifest+json"}},
      manifest_string);
}

void TestSignedWebBundleBuilder::AddPngImage(base::StringPiece url,
                                             base::StringPiece image_string) {
  // TODO(crbug.com/1385393): Remove base URL once relative URL is supported.
  GURL base_url = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                      web_package::SignedWebBundleId::CreateForEd25519PublicKey(
                          (key_pair_.public_key)))
                      .origin()
                      .GetURL();
  builder_.AddExchange(base_url.Resolve(url),
                       {{":status", "200"}, {"content-type", "image/png"}},
                       image_string);
}

TestSignedWebBundle TestSignedWebBundleBuilder::Build() {
  return TestSignedWebBundle(
      web_package::WebBundleSigner::SignBundle(builder_.CreateBundle(),
                                               {key_pair_}),
      web_package::SignedWebBundleId::CreateForEd25519PublicKey(
          key_pair_.public_key));
}

TestSignedWebBundle BuildDefaultTestSignedWebBundle() {
  TestSignedWebBundleBuilder builder = TestSignedWebBundleBuilder(
      web_package::WebBundleSigner::KeyPair(kTestPublicKey, kTestPrivateKey));
  builder.AddManifest(kTestManifest);
  builder.AddPngImage(kTestIconUrl, GetTestIconInString());
  return builder.Build();
}
}  // namespace web_app
