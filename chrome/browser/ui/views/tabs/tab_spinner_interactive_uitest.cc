// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/perf/performance_test.h"
#include "chrome/test/base/ui_test_utils.h"

static const int kNumTabs = 4;

// Gauge performance of the tab spinner animation.
class TabSpinnerTest : public UIPerformanceTest {
 public:
  TabSpinnerTest() {
    // Disable hover cards to ensure they never get painted. Prevent scenarios
    // where the mouse may accidentally trigger a hover card by disabling hover
    // cards altogether.
    scoped_feature_list_.InitAndDisableFeature(features::kTabHoverCards);

    test_page_url_ = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("perf/tab-spinner-case.html")));
  }

  ~TabSpinnerTest() override = default;

  void IgnorePriorHistogramSamples() {
    // Take the snapshot delta; so that the samples created so far will be
    // eliminated from the samples.
    for (const auto& name : GetUMAHistogramNames()) {
      auto* histogram = base::StatisticsRecorder::FindHistogram(name);
      if (!histogram)
        continue;
      histogram->SnapshotDelta();
    }
  }

  GURL test_page_url() const { return test_page_url_; }

 private:
  std::vector<std::string> GetUMAHistogramNames() const override {
    // This used to report the different stages from the pipline, but they have
    // been removed for the UI compositor. Details in crbug.com/1005226
    return {};
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  GURL test_page_url_;

  DISALLOW_COPY_AND_ASSIGN(TabSpinnerTest);
};

// TODO(crbug.com/974349) This test is timing out on all platforms
IN_PROC_BROWSER_TEST_F(TabSpinnerTest, DISABLED_LoadTabsOneByOne) {
  IgnorePriorHistogramSamples();

  // Navigate to a custom page that takes 10 seconds to load. Wait for the
  // tab to finish loading before opening a new tab. Repeat the process for
  // each tab.

  for (int i = 0; i < kNumTabs; ++i) {
    WindowOpenDisposition disposition =
        i == 0 ? WindowOpenDisposition::CURRENT_TAB
               : WindowOpenDisposition::NEW_FOREGROUND_TAB;
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), test_page_url(), disposition,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }
}

IN_PROC_BROWSER_TEST_F(TabSpinnerTest, LoadTabsTogether) {
  IgnorePriorHistogramSamples();

  // Open each tab in quick succession, all of which are simultaneously loading
  // the same custom page which takes 10 seconds to load. Wait for navigation
  // to finish on the last tab opened. We may assume the last tab opened is the
  // last tab to finish executing BROWSER_TEST_WAIT_FOR_NAVIGATION because it
  // is the last to navigate to the page.

  for (int i = 0; i < kNumTabs; ++i) {
    WindowOpenDisposition disposition =
        i == 0 ? WindowOpenDisposition::CURRENT_TAB
               : WindowOpenDisposition::NEW_FOREGROUND_TAB;
    ui_test_utils::BrowserTestWaitFlags flag =
        i < kNumTabs - 1 ? ui_test_utils::BROWSER_TEST_NONE
                         : ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION;

    ui_test_utils::NavigateToURLWithDisposition(browser(), test_page_url(),
                                                disposition, flag);
  }
}
