// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Eq;
using ::testing::StartsWith;

namespace web_app {

namespace {
constexpr std::string_view kTypeApp = "app";
constexpr std::string_view kIsolatedAppName = "Simple Isolated App";
constexpr std::string_view kIsolatedAppVersion = "1.0.0";
constexpr std::string_view kIsolatedAppDevToolsTitle =
    "Simple Isolated App (1.0.0)";
}
class IsolatedWebAppDevToolsTest : public IsolatedWebAppBrowserTestHarness {
 protected:
  IsolatedWebAppUrlInfo InstallIsolatedWebApp() {
    auto app =
        web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                           .SetName(kIsolatedAppName)
                                           .SetVersion(kIsolatedAppVersion))
            .BuildBundle();
    app->TrustSigningKey();
    return app->InstallChecked(profile());
  }
};

// TODO (crbug.com/1522953): Resolve flakiness on linux debug builds.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_ErrorPage DISABLED_ErrorPage
#else
#define MAYBE_ErrorPage ErrorPage
#endif
IN_PROC_BROWSER_TEST_F(IsolatedWebAppDevToolsTest, MAYBE_ErrorPage) {
  std::unique_ptr<net::EmbeddedTestServer> server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  IsolatedWebAppUrlInfo url_info =
      InstallDevModeProxyIsolatedWebApp(server->GetOrigin());
  Browser* browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  DevToolsWindowTesting::OpenDevToolsWindowSync(web_contents,
                                                /*is_docked=*/true);

  content::TestNavigationObserver navigation_observer(web_contents);
  EXPECT_TRUE(server->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(ExecJs(web_contents, "location.reload()"));
  navigation_observer.Wait();

  GURL expected_url = url_info.origin().GetURL().Resolve("/index.html");
  EXPECT_THAT(navigation_observer.last_navigation_url(), Eq(expected_url));
  EXPECT_THAT(navigation_observer.last_net_error_code(),
              Eq(net::ERR_CONNECTION_REFUSED));
  content::RenderFrameHost* error_frame = web_contents->GetPrimaryMainFrame();
  EXPECT_TRUE(error_frame->GetLastCommittedOrigin().opaque());
  EXPECT_THAT(error_frame->GetStoragePartition(),
              Eq(profile()->GetDefaultStoragePartition()));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppDevToolsTest, IwaIdentifiedAsApp) {
  // 1) Install an Isolated Web App and check its type in DevTools
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  Browser* iwa_app = LaunchWebAppBrowserAndWait(url_info.app_id());
  scoped_refptr<content::DevToolsAgentHost> iwa_host =
      content::DevToolsAgentHost::GetOrCreateFor(
          iwa_app->tab_strip_model()->GetActiveWebContents());
  const std::string& iwa_type = iwa_host->GetType();
  EXPECT_EQ(kTypeApp, iwa_type);

  // 2) Navigate to the Chrome Inspect page and check its js content
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(1, content::EvalJs(
                   web_contents,
                   "document.getElementById('apps-list').children.length"));
  EXPECT_EQ(kIsolatedAppDevToolsTitle,
            content::EvalJs(web_contents,
                            "document.getElementById('apps-list').children[0]."
                            "getElementsByClassName('name')[0].innerText"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppDevToolsTest, IwaWithCorrectTitle) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  Browser* iwa_app = LaunchWebAppBrowserAndWait(url_info.app_id());
  scoped_refptr<content::DevToolsAgentHost> iwa_host =
      content::DevToolsAgentHost::GetOrCreateFor(
          iwa_app->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(iwa_host->GetType(), kTypeApp);
  EXPECT_EQ(iwa_host->GetTitle(), kIsolatedAppDevToolsTitle);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppDevToolsTest, PwaIdentifiedAsPage) {
  // 1) Regression test to install PWA and make sure they still show as page
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  webapps::AppId pwa_id = web_app::test::InstallPwaForCurrentUrl(browser());
  Browser* pwa_app =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), pwa_id);
  scoped_refptr<content::DevToolsAgentHost> pwa_host =
      content::DevToolsAgentHost::GetOrCreateFor(
          pwa_app->tab_strip_model()->GetActiveWebContents());
  const std::string& pwa_type = pwa_host->GetType();
  EXPECT_EQ(content::DevToolsAgentHost::kTypePage, pwa_type);

  // 2) Navigate to the Chrome Inspect page and check its js content
  //    App list should be empty since PWA is identified as a page
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(0, content::EvalJs(
                   web_contents,
                   "document.getElementById('apps-list').children.length"));
}

}  // namespace web_app
