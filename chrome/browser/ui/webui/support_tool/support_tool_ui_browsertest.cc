// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/support_tool/support_tool_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

struct OpenSupportToolTestParam {
  std::string url;
  SupportToolWebUIActionType action;
};

const auto kParams = testing::Values(
    OpenSupportToolTestParam{
        content::GetWebUIURLString(chrome::kChromeUISupportToolHost),
        SupportToolWebUIActionType::kOpenSupportToolPage},
    OpenSupportToolTestParam{
        content::GetWebUIURLString(chrome::kChromeUISupportToolHost) + "/",
        SupportToolWebUIActionType::kOpenSupportToolPage},
    OpenSupportToolTestParam{
        content::GetWebUIURLString(chrome::kChromeUISupportToolHost) +
            "/url-generator/",
        SupportToolWebUIActionType::kOpenURLGeneratorPage},
    OpenSupportToolTestParam{
        content::GetWebUIURLString(chrome::kChromeUISupportToolHost) +
            "/url-generator",
        SupportToolWebUIActionType::kOpenURLGeneratorPage},
    // The main Support Tool page will be shown for invalid paths.
    OpenSupportToolTestParam{
        content::GetWebUIURLString(chrome::kChromeUISupportToolHost) +
            "/invalid-path",
        SupportToolWebUIActionType::kOpenSupportToolPage});

class SupportToolUIBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<OpenSupportToolTestParam> {
 public:
  SupportToolUIBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSupportTool);
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// Verifies that the WebUI page loads and the corresponding metrics are
// collected.
IN_PROC_BROWSER_TEST_P(SupportToolUIBrowserTest, OpenSupportToolWebUI) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(GetParam().url)));
  histogram_tester_.ExpectUniqueSample(kSupportToolWebUIActionHistogram,
                                       GetParam().action, 1);
}

INSTANTIATE_TEST_SUITE_P(, SupportToolUIBrowserTest, kParams);
