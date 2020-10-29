// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_WIN)
#include "chrome/browser/webshare/win/scoped_share_operation_fake_components.h"
#endif

class ShareServiceBrowserTest : public InProcessBrowserTest {
 public:
  ShareServiceBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kWebShare);
  }

#if defined(OS_WIN)
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    if (!IsSupportedEnvironment())
      return;

    ASSERT_NO_FATAL_FAILURE(scoped_fake_components_.SetUp());
  }
#endif

 protected:
#if defined(OS_WIN)
  bool IsSupportedEnvironment() {
    return webshare::ScopedShareOperationFakeComponents::
        IsSupportedEnvironment();
  }
#endif

 private:
  base::test::ScopedFeatureList feature_list_;
#if defined(OS_WIN)
  webshare::ScopedShareOperationFakeComponents scoped_fake_components_;
#endif
};

IN_PROC_BROWSER_TEST_F(ShareServiceBrowserTest, Text) {
#if defined(OS_WIN)
  if (!IsSupportedEnvironment())
    return;
#endif

  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/webshare/index.html"));

  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const std::string script = "share_text('hello')";
  const content::EvalJsResult result = content::EvalJs(contents, script);

#if defined(OS_CHROMEOS)
  // ChromeOS currently only supports file sharing.
  EXPECT_EQ("share failed: AbortError: Share canceled", result);
#else
  EXPECT_EQ("share succeeded", result);
#endif
}
