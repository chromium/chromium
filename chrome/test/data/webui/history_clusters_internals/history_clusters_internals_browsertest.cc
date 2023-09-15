// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/url_constants.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "content/public/test/browser_test.h"

class HistoryClustersInternalsDisabledBrowserTest
    : public WebUIMochaBrowserTest {
 protected:
  HistoryClustersInternalsDisabledBrowserTest() {
    set_test_loader_host(
        history_clusters_internals::kChromeUIHistoryClustersInternalsHost);
  }

 protected:
  void OnWebContentsAvailable(content::WebContents* web_contents) override {
    // Additional setup steps that need to happen after the page has loaded and
    // before the Mocha test runs.
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);

    // Trigger the debug messages to be added to the internals page.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(history_clusters::GetChromeUIHistoryClustersURL())));
  }

  void RunTestCase(const std::string& testCase) {
    RunTestWithoutTestLoader(
        "history_clusters_internals/history_clusters_internals_test.js",
        base::StringPrintf(
            "runMochaTest('HistoryClustersInternalsTest', '%s');",
            testCase.c_str()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      history_clusters::internal::kJourneys};
};

IN_PROC_BROWSER_TEST_F(HistoryClustersInternalsDisabledBrowserTest,
                       InternalsPageFeatureDisabled) {
  RunTestCase("InternalsPageFeatureDisabled");
}

class HistoryClustersInternalsBrowserTest
    : public HistoryClustersInternalsDisabledBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list2_{
      history_clusters::internal::kHistoryClustersInternalsPage};
};

IN_PROC_BROWSER_TEST_F(HistoryClustersInternalsBrowserTest,
                       InternalsPageFeatureEnabled) {
  RunTestCase("InternalsPageFeatureEnabled");
}
