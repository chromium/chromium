// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/compute_app_size_command.h"

#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

class ComputeAppSizeCommandBrowserTest : public WebAppBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(ComputeAppSizeCommandBrowserTest, RetrieveWebAppSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  const char* script = R"(
        localStorage.setItem('data', 'data'.repeat(5000));
        location.href = 'about:blank';
        true;
      )";

  EXPECT_TRUE(
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), script)
          .ExtractBool());

  // We need to wait for the quota manager to receive storage data from the
  // renderer process. As updates to quota manager usage occurs on a different
  // sequence to this procress, it requires multiple events. Due to all of this,
  // we are resorting to polling for non-zero values.
  while (true) {
    base::test::TestFuture<std::optional<ComputedAppSize>> app_size;
    provider().scheduler().ComputeAppSize(app_id, app_size.GetCallback());
    if (app_size.Get().value().app_size_in_bytes != 0u &&
        app_size.Get().value().data_size_in_bytes != 0u) {
      break;
    }
  }
}

}  // namespace web_app
