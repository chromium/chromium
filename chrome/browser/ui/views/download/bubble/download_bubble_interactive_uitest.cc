// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class DownloadBubbleInteractiveUiTest : public DownloadTestBase,
                                        public InteractiveBrowserTestApi {
 public:
  DownloadBubbleInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2}, {});
  }

  void SetUpOnMainThread() override {
    DownloadTestBase::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(embedded_test_server()->Start());
    private_test_impl().DoTestSetUp();
    SetContextWidget(
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());
  }

  void TearDownOnMainThread() override {
    SetContextWidget(nullptr);
    private_test_impl().DoTestTearDown();
    DownloadTestBase::TearDownOnMainThread();
  }

  auto DownloadTestFile() {
    GURL url = embedded_test_server()->GetURL(
        base::StrCat({"/", DownloadTestBase::kDownloadTest1Path}));
    return base::BindLambdaForTesting(
        [this, url]() { DownloadAndWait(browser(), url); });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       ToolbarIconShownAfterDownload) {
  RunTestSequence(Do(DownloadTestFile()),
                  WaitForShow(kDownloadToolbarButtonElementId));
}

}  // namespace
