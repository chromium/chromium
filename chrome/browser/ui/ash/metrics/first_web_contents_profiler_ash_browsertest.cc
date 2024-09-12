// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

class FirstNonEmptyPaintObserver : public content::WebContentsObserver {
 public:
  FirstNonEmptyPaintObserver(base::HistogramTester* histogram_tester,
                             Browser* browser)
      : content::WebContentsObserver(
            browser->tab_strip_model()->GetActiveWebContents()),
        histogram_tester_(histogram_tester) {
    CHECK(histogram_tester_);
  }
  FirstNonEmptyPaintObserver(const FirstNonEmptyPaintObserver&) = delete;
  FirstNonEmptyPaintObserver& operator=(const FirstNonEmptyPaintObserver&) =
      delete;
  ~FirstNonEmptyPaintObserver() override = default;

  // Returns true if the first non-empty paint has been received; false if
  // timed out.
  bool Wait() {
    static constexpr base::TimeDelta kTimeout = base::Seconds(10);
    // May have already been recorded if the session restore/browser paint
    // completed during the test's setup.
    if (histogram_tester_->GetTotalSum(
            "Startup.FirstWebContents.NonEmptyPaint3") > 0) {
      return true;
    }
    if (is_complete_) {
      return true;
    }
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::OneShotTimer timeout;
    timeout.Start(FROM_HERE, kTimeout, this,
                  &FirstNonEmptyPaintObserver::QuitAndTimeout);
    run_loop.Run();
    quit_closure_.Reset();
    return is_complete_;
  }

 private:
  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override {
    is_complete_ = true;
    if (quit_closure_) {
      quit_closure_.Run();
    }
  }

  void QuitAndTimeout() {
    CHECK(quit_closure_);
    quit_closure_.Run();
  }

  const raw_ptr<base::HistogramTester> histogram_tester_;
  bool is_complete_ = false;
  base::RepeatingClosure quit_closure_;
};

class FirstWebContentsProfilerAshTest : public InProcessBrowserTest {
 public:
  FirstWebContentsProfilerAshTest() { set_launch_browser_for_testing(nullptr); }
  FirstWebContentsProfilerAshTest(const FirstWebContentsProfilerAshTest&) =
      delete;
  FirstWebContentsProfilerAshTest& operator=(
      const FirstWebContentsProfilerAshTest&) = delete;
  ~FirstWebContentsProfilerAshTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    prefs->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                      static_cast<int>(full_restore::RestoreOption::kAlways));
  }

 protected:
  base::HistogramTester histogram_tester_;
};

// Creates browser window that will be restored in the main test.
IN_PROC_BROWSER_TEST_F(FirstWebContentsProfilerAshTest,
                       PRE_RecordsFirstWebContentsMetricsOnRestore) {
  ASSERT_TRUE(BrowserList::GetInstance()->empty());

  FirstNonEmptyPaintObserver first_non_empty_paint_observer(
      &histogram_tester_,
      CreateBrowser(ProfileManager::GetActiveUserProfile()));
  ASSERT_TRUE(first_non_empty_paint_observer.Wait());
  // Metrics are not recorded because the browser window is manually
  // created not from session restore.
  histogram_tester_.ExpectTotalCount("Startup.FirstWebContents.NonEmptyPaint3",
                                     0);
  histogram_tester_.ExpectTotalCount("Ash.FirstWebContentsProfile.Recorded", 0);

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Browser window is restored during test setup, so "Startup.FirstWebContents"
// metrics should be recorded.
IN_PROC_BROWSER_TEST_F(FirstWebContentsProfilerAshTest,
                       RecordsFirstWebContentsMetricsOnRestore) {
  ASSERT_TRUE(BrowserList::GetInstance()->GetLastActive());
  FirstNonEmptyPaintObserver first_non_empty_paint_observer(
      &histogram_tester_, BrowserList::GetInstance()->GetLastActive());
  ASSERT_TRUE(first_non_empty_paint_observer.Wait());
  histogram_tester_.ExpectTotalCount(
      "Startup.FirstWebContents.MainNavigationStart", 1);
  histogram_tester_.ExpectTotalCount(
      "Startup.FirstWebContents.MainNavigationFinished", 1);
  histogram_tester_.ExpectTotalCount("Startup.FirstWebContents.NonEmptyPaint3",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "Startup.FirstWebContents.FinishReason",
      /*metrics::StartupProfilingFinishReason::kDone*/ 0, 1);
  histogram_tester_.ExpectUniqueSample("Ash.FirstWebContentsProfile.Recorded",
                                       true, 1);
}

}  // namespace
}  // namespace ash
