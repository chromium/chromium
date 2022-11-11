// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"

#include "base/files/file_path.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace web_app {

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
      url_info, IsolationData{IsolationData::DevModeProxy{.proxy_url = origin}},
      future.GetCallback());

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
      WebAppProvider::GetForWebApps(profile())->registrar();
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

}  // namespace web_app
