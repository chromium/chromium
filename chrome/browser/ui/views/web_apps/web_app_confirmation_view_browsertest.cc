// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

class WebAppConfirmViewBrowserTest
    : public DialogBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  WebAppConfirmViewBrowserTest() = default;
  WebAppConfirmViewBrowserTest(const WebAppConfirmViewBrowserTest&) = delete;
  WebAppConfirmViewBrowserTest& operator=(const WebAppConfirmViewBrowserTest&) =
      delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto app_info = std::make_unique<WebApplicationInfo>();
    app_info->title = u"Test app";
    app_info->start_url = GURL("https://example.com");

    auto callback = [](bool result, std::unique_ptr<WebApplicationInfo>) {};

    chrome::ShowWebAppInstallDialog(
        browser()->tab_strip_model()->GetActiveWebContents(),
        std::move(app_info), base::BindLambdaForTesting(callback));
  }

  void SetUp() override {
    if (GetParam()) {
      feature_list.InitWithFeatures({features::kDesktopPWAsTabStrip,
                                     features::kDesktopPWAsTabStripSettings},
                                    {});
    } else {
      feature_list.InitWithFeatures({},
                                    {features::kDesktopPWAsTabStrip,
                                     features::kDesktopPWAsTabStripSettings});
    }
    DialogBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_P(WebAppConfirmViewBrowserTest, ShowWebAppInstallDialog) {
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->title = u"Test app";
  app_info->start_url = GURL("https://example.com");

  chrome::SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                              /*auto_open_in_window=*/true);
  bool is_accepted = false;
  auto callback = [&is_accepted](bool result,
                                 std::unique_ptr<WebApplicationInfo>) {
    is_accepted = result;
  };

  chrome::ShowWebAppInstallDialog(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::BindLambdaForTesting(callback));
  EXPECT_TRUE(is_accepted);
}

IN_PROC_BROWSER_TEST_P(WebAppConfirmViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppConfirmViewBrowserTest,
                         ::testing::Values(false, true));
