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
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
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

class IsolatedWebAppDevToolsTest : public IsolatedWebAppBrowserTestHarness {
 protected:
  IsolatedWebAppUrlInfo InstallIsolatedWebApp() {
    server_ =
        CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
    web_app::IsolatedWebAppUrlInfo url_info =
        InstallDevModeProxyIsolatedWebApp(server_->GetOrigin());
    return url_info;
  }

  net::EmbeddedTestServer* dev_server() { return server_.get(); }

  WebAppProvider& web_app_provider() {
    return CHECK_DEREF(WebAppProvider::GetForTest(profile()));
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> server_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppDevToolsTest, ErrorPage) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  Browser* browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  DevToolsWindowTesting::OpenDevToolsWindowSync(web_contents,
                                                /*is_docked=*/true);

  content::TestNavigationObserver navigation_observer(web_contents);
  EXPECT_TRUE(dev_server()->ShutdownAndWaitUntilComplete());
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

}  // namespace web_app
