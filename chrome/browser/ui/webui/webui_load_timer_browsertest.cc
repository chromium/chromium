// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_load_timer.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kTestUrl[] = "chrome://chrome-urls";
const char kDocumentInitialLoadUmaId[] = "document_initial_load";
const char kDocumentLoadCompletedUmaId[] = "document_load_completed";

}  // namespace

using WebuiLoadTimerTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(WebuiLoadTimerTest, Timers) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTestUrl)));
  WebuiLoadTimer timer(browser()->tab_strip_model()->GetActiveWebContents(),
                       kDocumentInitialLoadUmaId, kDocumentLoadCompletedUmaId);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));

  histogram_tester.ExpectTotalCount(kDocumentInitialLoadUmaId, 1);
  histogram_tester.ExpectTotalCount(kDocumentLoadCompletedUmaId, 1);
}

class WebuiLoadTimerPrerenderTest : public WebuiLoadTimerTest {
 public:
  WebuiLoadTimerPrerenderTest()
      : prerender_helper_(
            base::BindRepeating(&WebuiLoadTimerPrerenderTest::web_contents,
                                base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(WebuiLoadTimerPrerenderTest,
                       TimersOnPrerenderNavigation) {
  base::HistogramTester histogram_tester;
  WebuiLoadTimer timer(web_contents(), kDocumentInitialLoadUmaId,
                       kDocumentLoadCompletedUmaId);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html")));
  histogram_tester.ExpectTotalCount(kDocumentInitialLoadUmaId, 1);
  histogram_tester.ExpectTotalCount(kDocumentLoadCompletedUmaId, 1);

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  prerender_helper_.AddPrerender(prerender_url);
  // Ensure prerender navigation doesn't update WebuiLoadTimer.
  histogram_tester.ExpectTotalCount(kDocumentInitialLoadUmaId, 1);
  histogram_tester.ExpectTotalCount(kDocumentLoadCompletedUmaId, 1);

  // Activate a prerendered page.
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  // Ensure WebuiLoadTimer reports page load times.
  histogram_tester.ExpectTotalCount(kDocumentInitialLoadUmaId, 2);
  histogram_tester.ExpectTotalCount(kDocumentLoadCompletedUmaId, 2);
}
