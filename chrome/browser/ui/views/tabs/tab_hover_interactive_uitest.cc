// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/perf/drag_event_generator.h"
#include "chrome/test/base/perf/performance_test.h"
#include "chrome/test/base/ui_test_utils.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/wm/public/scoped_tooltip_disabler.h"
#endif

using views::Widget;

static const int kNumTabs = 8;

// TODO(crbug.com/991000): Enable this test for Windows and Linux once
// DragEventGenerator works for those platforms as well.
class TabHoverTest : public UIPerformanceTest {
 public:
  TabHoverTest() {
    // Disable hover cards.
    scoped_feature_list_.InitAndDisableFeature(features::kTabHoverCards);
  }

  ~TabHoverTest() override = default;

  void SetUpOnMainThread() override {
    // Disable tooltips. Note that this only works if we are using Aura.
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
#if defined(USE_AURA)
    aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
    tooltip_disabler_ =
        std::make_unique<wm::ScopedTooltipDisabler>(browser_window);
#endif

    // Open up the tabs first so we only trace after they're open.
    const GURL ntp_url("about:blank");
    for (int i = 1; i < kNumTabs; ++i) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), ntp_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    }

    // Now start the trace.
    UIPerformanceTest::SetUpOnMainThread();
  }

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

 private:
  std::vector<std::string> GetUMAHistogramNames() const override {
    // This used to report the different stages from the pipline, but they have
    // been removed for the UI compositor. Details in crbug.com/1005226
    return {};
  }

  base::test::ScopedFeatureList scoped_feature_list_;
#if defined(USE_AURA)
  std::unique_ptr<wm::ScopedTooltipDisabler> tooltip_disabler_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TabHoverTest);
};

IN_PROC_BROWSER_TEST_F(TabHoverTest, HoverOverMultipleTabs) {
  // This test is intended to gauge performance of the tab strip during
  // tab hovering by mousing over each tab in the window, from left to right.
  // This is meant to mimic a user looking at each tab in order to locate
  // a specific one.

  IgnorePriorHistogramSamples();

  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();

  // Start at the center of the first tab.
  const gfx::Point start_position =
      ui_test_utils::GetCenterInScreenCoordinates(tab_strip->tab_at(0));

  // End at the center of the last tab.
  const gfx::Point end_position = ui_test_utils::GetCenterInScreenCoordinates(
      tab_strip->tab_at(kNumTabs - 1));

  // Slowly mouse from the start to end positions across the tab strip. Tick
  // this mousemove at a high frequency (120fps) to avoid having the timer fire
  // at the wrong time due to having frames without any input event.
  auto generator = ui_test_utils::DragEventGenerator::CreateForMouse(
      std::make_unique<ui_test_utils::InterpolatedProducer>(
          start_position, end_position,
          base::TimeDelta::FromMilliseconds(5000)),
      /*hover=*/true, /*use_120fpx=*/true);
  generator->Wait();

#if defined(USE_AURA)
  const gfx::Point& last_mouse_loc =
      aura::Env::GetInstance()->last_mouse_location();
  ASSERT_EQ(end_position, last_mouse_loc);
#endif
}
