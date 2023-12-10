// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

using EventLatencyMetricsTest = InProcessBrowserTest;

// Ash.EventLatency metrics should not be recorded when the target window
// is backing web contents.
// Disabled due to flakes; see https://crbug.com/1504093.
IN_PROC_BROWSER_TEST_F(EventLatencyMetricsTest,
                       DISABLED_NoReportForWebContents) {
  base::HistogramTester histogram_tester;

  const GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("simple_textarea.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ("",
            content::EvalJs(tab, "document.getElementById('text_id').value;"));
  ASSERT_TRUE(
      content::ExecJs(tab, "document.getElementById('text_id').focus();"));
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetWidget()
                  ->GetCompositor());
  // Text area value has changed.
  EXPECT_EQ("a",
            content::EvalJs(tab, "document.getElementById('text_id').value;"));

  // Event latency metrics should not be recorded if the target window is
  // backing web contents.
  histogram_tester.ExpectTotalCount("Ash.EventLatency.KeyPressed.TotalLatency",
                                    0);
  histogram_tester.ExpectTotalCount("Ash.EventLatency.KeyReleased.TotalLatency",
                                    0);
  histogram_tester.ExpectTotalCount("Ash.EventLatency.TotalLatency", 0);
}

}  // namespace
}  // namespace ash
