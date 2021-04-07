// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/webshare/chromeos/sharesheet_client.h"
#endif
#if defined(OS_WIN)
#include "chrome/browser/webshare/win/scoped_share_operation_fake_components.h"
#endif
#if defined(OS_MAC)
#include "chrome/browser/webshare/mac/sharing_service_operation.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif

class ShareServiceBrowserTest : public InProcessBrowserTest {
 public:
  ShareServiceBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kWebShare);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    webshare::SharesheetClient::SetSharesheetCallbackForTesting(
        base::BindRepeating(&ShareServiceBrowserTest::AcceptShareRequest));
#endif
#if defined(OS_WIN)
    if (!IsSupportedEnvironment())
      return;

    ASSERT_NO_FATAL_FAILURE(scoped_fake_components_.SetUp());
#endif
#if defined(OS_MAC)
    webshare::SharingServiceOperation::SetSharePickerCallbackForTesting(
        base::BindRepeating(&ShareServiceBrowserTest::AcceptShareRequest));
#endif
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  static void AcceptShareRequest(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::vector<std::string>& content_types,
      const std::string& text,
      const std::string& title,
      sharesheet::DeliveredCallback delivered_callback) {
    std::move(delivered_callback).Run(sharesheet::SharesheetResult::kSuccess);
  }
#endif

#if defined(OS_MAC)
  static void AcceptShareRequest(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::string& text,
      const std::string& title,
      const GURL& url,
      blink::mojom::ShareService::ShareCallback close_callback) {
    std::move(close_callback).Run(blink::mojom::ShareError::OK);
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

  EXPECT_EQ("share succeeded", result);
}
