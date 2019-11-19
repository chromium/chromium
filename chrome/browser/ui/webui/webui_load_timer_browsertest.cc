// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_load_timer.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
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
  ui_test_utils::NavigateToURL(browser(), GURL(kTestUrl));
  WebuiLoadTimer timer(browser()->tab_strip_model()->GetActiveWebContents(),
                       kDocumentInitialLoadUmaId, kDocumentLoadCompletedUmaId);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  histogram_tester.ExpectTotalCount(kDocumentInitialLoadUmaId, 1);
  histogram_tester.ExpectTotalCount(kDocumentLoadCompletedUmaId, 1);
}
