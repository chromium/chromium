// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using ::testing::Eq;

namespace web_app {

// Evaluates to true if the test value is within 5% of the given value.
MATCHER_P(IsApproximately, approximate_value, "") {
  return arg > (approximate_value * 0.95) && arg < (approximate_value * 1.05);
}

class IsolatedWebAppBrowsingDataTest : public IsolatedWebAppBrowserTestHarness {
 protected:
  IsolatedWebAppBrowsingDataTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIwaControlledFrame);
  }

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

  int64_t GetIwaUsage(const IsolatedWebAppUrlInfo& url_info) {
    base::test::TestFuture<base::flat_map<url::Origin, int64_t>> future;
    web_app_provider().scheduler().GetIsolatedWebAppBrowsingData(
        future.GetCallback());
    base::flat_map<url::Origin, int64_t> result = future.Get();
    return result.contains(url_info.origin()) ? result.at(url_info.origin())
                                              : 0;
  }

  void AddUsageIfMissing(const content::ToRenderFrameHost& target) {
    EXPECT_TRUE(
        ExecJs(target, "localStorage.setItem('test', '!'.repeat(1000))"));

    base::test::TestFuture<void> test_future;
    target.render_frame_host()
        ->GetStoragePartition()
        ->GetLocalStorageControl()
        ->Flush(test_future.GetCallback());
    EXPECT_TRUE(test_future.Wait());
  }

  [[nodiscard]] bool CreateControlledFrame(content::WebContents* web_contents,
                                           const GURL& src,
                                           const std::string& partition) {
    static std::string kCreateControlledFrame = R"(
      (async function() {
        const controlledframe = document.createElement('controlledframe');
        controlledframe.setAttribute('src', $1);
        controlledframe.setAttribute('partition', $2);
        await new Promise((resolve) => {
          controlledframe.addEventListener('loadcommit', resolve);
          document.body.appendChild(controlledframe);
        });
      })();
    )";
    return ExecJs(web_contents,
                  content::JsReplace(kCreateControlledFrame, src, partition));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> server_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataTest,
                       ControlledFrameUsageIsCounted) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  Browser* browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_THAT(GetIwaUsage(url_info), Eq(0));

  // Add some usage to the IWA and make sure it's counted.
  AddUsageIfMissing(web_contents);
  EXPECT_THAT(GetIwaUsage(url_info), IsApproximately(1000));

  // Create a persisted <controlledframe>, add some usage to it.
  EXPECT_TRUE(CreateControlledFrame(web_contents,
                                    dev_server()->GetURL("/empty_title.html"),
                                    "persist:partition_name"));
  ASSERT_EQ(1UL, web_contents->GetInnerWebContents().size());
  AddUsageIfMissing(web_contents->GetInnerWebContents()[0]);
  EXPECT_THAT(GetIwaUsage(url_info), IsApproximately(2000));

  // Create another persisted <controlledframe> with a different partition name.
  EXPECT_TRUE(CreateControlledFrame(web_contents,
                                    dev_server()->GetURL("/empty_title.html"),
                                    "persist:partition_name_2"));
  ASSERT_EQ(2UL, web_contents->GetInnerWebContents().size());
  AddUsageIfMissing(web_contents->GetInnerWebContents()[0]);
  AddUsageIfMissing(web_contents->GetInnerWebContents()[1]);
  EXPECT_THAT(GetIwaUsage(url_info), IsApproximately(3000));

  // Create an in-memory <controlledframe> that won't count towards IWA usage.
  EXPECT_TRUE(CreateControlledFrame(
      web_contents, dev_server()->GetURL("/empty_title.html"), "unpersisted"));
  ASSERT_EQ(3UL, web_contents->GetInnerWebContents().size());
  AddUsageIfMissing(web_contents->GetInnerWebContents()[0]);
  AddUsageIfMissing(web_contents->GetInnerWebContents()[1]);
  AddUsageIfMissing(web_contents->GetInnerWebContents()[2]);
  EXPECT_THAT(GetIwaUsage(url_info), IsApproximately(3000));
}

}  // namespace web_app
