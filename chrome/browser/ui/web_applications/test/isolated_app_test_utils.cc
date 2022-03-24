// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/isolated_app_test_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace web_app {

IsolatedAppBrowserTestHarness::IsolatedAppBrowserTestHarness() = default;

IsolatedAppBrowserTestHarness::~IsolatedAppBrowserTestHarness() = default;

AppId IsolatedAppBrowserTestHarness::InstallIsolatedApp(
    const std::string& host) {
  GURL app_url = https_server()->GetURL(host,
                                        "/banners/manifest_test_page.html"
                                        "?manifest=manifest_isolated.json");
  return InstallIsolatedApp(app_url);
}

AppId IsolatedAppBrowserTestHarness::InstallIsolatedApp(const GURL& app_url) {
  EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), app_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  return test::InstallPwaForCurrentUrl(browser());
}

content::RenderFrameHost* IsolatedAppBrowserTestHarness::OpenApp(
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

content::RenderFrameHost* IsolatedAppBrowserTestHarness::NavigateToURLInNewTab(
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
