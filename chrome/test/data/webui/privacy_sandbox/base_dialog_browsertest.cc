// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {

enum class WindowSize { kSmall, kBig };

class PrivacySandboxBaseDialogMochaTest
    : public WebUIMochaBrowserTest,
      public testing::WithParamInterface<WindowSize> {
 protected:
  PrivacySandboxBaseDialogMochaTest() {
    set_test_loader_host(chrome::kChromeUIPrivacySandboxBaseDialogHost);
  }

  void RunTestSuite(const std::string& testCase) {
    if (GetParam() == WindowSize::kSmall) {
      ForceWindowSizeSmall();
    } else {
      ForceWindowSizeBig();
    }
    WebUIMochaBrowserTest::RunTest(
        "privacy_sandbox/base_dialog_test.js",
        base::StringPrintf("runMochaSuite('%s');", testCase.c_str()));
  }

 private:
  void ForceWindowSizeSmall() {
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, 620, 600});
  }

  void ForceWindowSizeBig() {
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, 620, 900});
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrivacySandboxBaseDialogMochaTest,
                         testing::Values(WindowSize::kSmall, WindowSize::kBig));

IN_PROC_BROWSER_TEST_P(PrivacySandboxBaseDialogMochaTest, TopicsConsentNotice) {
  RunTestSuite("TopicsConsentNotice");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxBaseDialogMochaTest,
                       ProtectedAudienceMeasurementNotice) {
  RunTestSuite("ProtectedAudienceMeasurementNotice");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxBaseDialogMochaTest, ThreeAdsApisNotice) {
  RunTestSuite("ThreeAdsApisNotice");
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxBaseDialogMochaTest, MeasurementNotice) {
  RunTestSuite("MeasurementNotice");
}

}  // namespace
