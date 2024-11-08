// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget.h"

namespace {

enum class WindowSize { kSmall, kBig };

}  // namespace

class PrivacySandboxDialogTest
    : public WebUIMochaBrowserTest,
      public testing::WithParamInterface<WindowSize> {
 protected:
  PrivacySandboxDialogTest() {
    set_test_loader_host(chrome::kChromeUIPrivacySandboxDialogHost);
  }

  void RunTestSuite(const std::string& testCase) {
    if (GetParam() == WindowSize::kSmall) {
      ForceWindowSizeSmall();
    } else {
      ForceWindowSizeBig();
    }
    WebUIMochaBrowserTest::RunTest(
        "privacy_sandbox/privacy_sandbox_dialog_test.js",
        base::StringPrintf("runMochaSuite('%s');", testCase.c_str()));
  }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 private:
  void ForceWindowSizeSmall() {
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, 620, 600});
  }

  void ForceWindowSizeBig() {
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, 620, 900});
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrivacySandboxDialogTest,
                         testing::Values(WindowSize::kSmall, WindowSize::kBig));

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogTest, Consent) {
  RunTestSuite("Consent");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogTest, Notice) {
  RunTestSuite("Notice");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogTest, Combined) {
  RunTestSuite("Combined");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogTest, NoticeEEA) {
  RunTestSuite("NoticeEEA");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogTest, NoticeROW) {
  RunTestSuite("NoticeROW");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogTest, NoticeRestricted) {
  RunTestSuite("NoticeRestricted");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogTest, Mixin) {
  RunTestSuite("Mixin");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogTest,
                       CombinedAdsApiUxEnhancementDisabled) {
  RunTestSuite("CombinedAdsApiUxEnhancementDisabled");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogTest, CombinedAdsApiUxEnhancement) {
  RunTestSuite("CombinedAdsApiUxEnhancement");
}
