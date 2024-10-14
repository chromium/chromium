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

class PrivacySandboxDialogTest : public WebUIMochaBrowserTest {
 protected:
  PrivacySandboxDialogTest() {
    set_test_loader_host(chrome::kChromeUIPrivacySandboxDialogHost);
  }

  void RunTestSuite(const std::string& testCase) {
    WebUIMochaBrowserTest::RunTest(
        "privacy_sandbox/privacy_sandbox_dialog_test.js",
        base::StringPrintf("runMochaSuite('%s');", testCase.c_str()));
  }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PrivacySandboxDialogSmallWindowTest : public PrivacySandboxDialogTest {
 protected:
  void ForceWindowSizeSmall() {
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, 620, 600});
  }

  void RunTestSuite(const std::string& testCase) {
    ForceWindowSizeSmall();
    PrivacySandboxDialogTest::RunTestSuite(testCase);
  }
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogSmallWindowTest, Consent) {
  RunTestSuite("Consent");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogSmallWindowTest, Notice) {
  RunTestSuite("Notice");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogSmallWindowTest, Combined) {
  RunTestSuite("Combined");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogSmallWindowTest, NoticeEEA) {
  RunTestSuite("NoticeEEA");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogSmallWindowTest, NoticeROW) {
  RunTestSuite("NoticeROW");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogSmallWindowTest, NoticeRestricted) {
  RunTestSuite("NoticeRestricted");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogSmallWindowTest, Mixin) {
  RunTestSuite("Mixin");
}

class PrivacySandboxDialogBigWindowTest : public PrivacySandboxDialogTest {
 protected:
  void ForceWindowSizeBig() {
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, 620, 900});
  }

  void RunTestSuite(const std::string& testCase) {
    ForceWindowSizeBig();
    PrivacySandboxDialogTest::RunTestSuite(testCase);
  }
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogBigWindowTest, Consent) {
  RunTestSuite("Consent");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogBigWindowTest, Notice) {
  RunTestSuite("Notice");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogBigWindowTest, Combined) {
  RunTestSuite("Combined");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogBigWindowTest, NoticeEEA) {
  RunTestSuite("NoticeEEA");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogBigWindowTest, NoticeROW) {
  RunTestSuite("NoticeROW");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogBigWindowTest, NoticeRestricted) {
  RunTestSuite("NoticeRestricted");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogBigWindowTest, Mixin) {
  RunTestSuite("Mixin");
}
