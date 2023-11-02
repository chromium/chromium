// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
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
    auto app_info = std::make_unique<WebAppInstallInfo>();
    app_info->title = u"Test app";
    app_info->start_url = GURL("https://example.com");

    auto callback = [](bool result, std::unique_ptr<WebAppInstallInfo>) {};

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
  auto app_info = std::make_unique<WebAppInstallInfo>();
  app_info->title = u"Test app";
  app_info->start_url = GURL("https://example.com");

  chrome::SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                              /*auto_open_in_window=*/true);
  bool is_accepted = false;
  auto callback = [&is_accepted](bool result,
                                 std::unique_ptr<WebAppInstallInfo>) {
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

IN_PROC_BROWSER_TEST_P(WebAppConfirmViewBrowserTest, NormalizeTitles) {
  chrome::SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                              /*auto_open_in_window=*/true);

  struct TestCases {
    std::u16string input;
    std::u16string expected_result;
  } test_cases[] = {
      {u"App Title", u"App Title"},
      {u"http://example.com", u"example.com"},
      {u"https://example.com", u"example.com"},
  };

  for (const TestCases& test_case : test_cases) {
    auto app_info = std::make_unique<WebAppInstallInfo>();
    app_info->title = test_case.input;
    app_info->start_url = GURL("https://example.com");

    bool is_accepted = false;
    std::u16string title;
    auto callback = [&is_accepted, &title](
                        bool result, std::unique_ptr<WebAppInstallInfo> info) {
      is_accepted = result;
      title = info->title;
    };

    chrome::ShowWebAppInstallDialog(
        browser()->tab_strip_model()->GetActiveWebContents(),
        std::move(app_info), base::BindLambdaForTesting(callback));
    EXPECT_TRUE(is_accepted) << test_case.input;
    EXPECT_EQ(test_case.expected_result, title) << test_case.input;
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppConfirmViewBrowserTest,
                         ::testing::Values(false, true));
