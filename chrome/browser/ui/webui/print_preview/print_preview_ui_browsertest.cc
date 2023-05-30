// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_metrics.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "printing/buildflags/buildflags.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

using task_manager::browsertest_util::MatchAboutBlankTab;
using task_manager::browsertest_util::MatchAnyPrint;
using task_manager::browsertest_util::MatchAnyTab;
using task_manager::browsertest_util::MatchPrint;
using task_manager::browsertest_util::WaitForTaskManagerRows;

namespace {

class PrintPreviewBrowserTest : public InProcessBrowserTest {
 public:
  PrintPreviewBrowserTest() = default;
  ~PrintPreviewBrowserTest() override = default;

  void Print() {
    content::TestNavigationObserver nav_observer(nullptr);
    nav_observer.StartWatchingNewWebContents();
    chrome::ExecuteCommand(browser(), IDC_PRINT);
    nav_observer.Wait();
    nav_observer.StopWatchingNewWebContents();
    EXPECT_EQ(GURL("chrome://print/"), nav_observer.last_navigation_url());
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewBrowserTest, PrintCommands) {
  // We start off at about:blank page.
  // Make sure there is 1 tab and print is enabled.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_PRINT));

  ASSERT_EQ(BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG),
            chrome::IsCommandEnabled(browser(), IDC_BASIC_PRINT));

  // Create the print preview dialog.
  Print();

  ASSERT_FALSE(chrome::IsCommandEnabled(browser(), IDC_PRINT));

  ASSERT_EQ(BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG),
            chrome::IsCommandEnabled(browser(), IDC_BASIC_PRINT));

  content::TestNavigationObserver reload_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  reload_observer.Wait();

  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_PRINT));

  ASSERT_EQ(BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG),
            chrome::IsCommandEnabled(browser(), IDC_BASIC_PRINT));
}

// Disable the test for mac, see http://crbug/367665.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TaskManagerNewPrintPreview DISABLED_TaskManagerNewPrintPreview
#else
#define MAYBE_TaskManagerNewPrintPreview TaskManagerNewPrintPreview
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewBrowserTest,
                       MAYBE_TaskManagerNewPrintPreview) {
  chrome::ShowTaskManager(browser());  // Show task manager BEFORE print dialog.

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyPrint()));

  // Create the print preview dialog.
  Print();

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrint()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchPrint(url::kAboutBlankURL)));
}

// http://crbug/367665.
IN_PROC_BROWSER_TEST_F(PrintPreviewBrowserTest,
                       DISABLED_TaskManagerExistingPrintPreview) {
  // Create the print preview dialog.
  Print();

  chrome::ShowTaskManager(browser());  // Show task manager AFTER print dialog.

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrint()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchPrint(url::kAboutBlankURL)));
}

#if BUILDFLAG(IS_WIN)
// http://crbug.com/396360
IN_PROC_BROWSER_TEST_F(PrintPreviewBrowserTest,
                       DISABLED_NoCrashOnCloseWithOtherTabs) {
  // Now print preview.
  Print();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  // Navigate main tab to hide print preview.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(PrintPreviewBrowserTest, PreviewStartedMetric) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      "PrintPreview.UserAction", printing::UserActionBuckets::kPreviewStarted,
      /*expected_count=*/0);

  Print();
  histogram_tester.ExpectBucketCount(
      "PrintPreview.UserAction", printing::UserActionBuckets::kPreviewStarted,
      /*expected_count=*/1);

  // Watch for the next navigation in the print preview dialog. The metric
  // shouldn't change. See crbug.com/1075795 and crbug.com/1448984.
  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.WatchExistingWebContents();
  nav_observer.Wait();
  EXPECT_EQ(GURL("chrome-untrusted://print/1/0/print.pdf"),
            nav_observer.last_navigation_url());
  histogram_tester.ExpectBucketCount(
      "PrintPreview.UserAction", printing::UserActionBuckets::kPreviewStarted,
      /*expected_count=*/1);
}

}  // namespace
