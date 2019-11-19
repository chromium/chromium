// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/performance_test.h"
#include "content/public/test/test_utils.h"
#include "ui/base/test/ui_controls.h"

// TODO(oshima): Add tablet mode overview transition.
class LauncherAnimationsTestBase : public UIPerformanceTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  LauncherAnimationsTestBase() = default;
  ~LauncherAnimationsTestBase() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();

    test::PopulateDummyAppListItems(100);
    if (base::SysInfo::IsRunningOnChromeOS()) {
      base::RunLoop run_loop;
      base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                            base::TimeDelta::FromSeconds(5));
      run_loop.Run();
    }

    const bool reuse_widget = GetParam();
    if (reuse_widget)
      CreateCachedWidget();
  }

  std::vector<std::string> GetUMAHistogramNames() const override {
    const std::string suffix = GetAnimationSmoothnessMetricsName();
    DCHECK(!suffix.empty());
    std::vector<std::string> names{
        "Apps.StateTransition.AnimationSmoothness." + suffix,
        "Apps.StateTransition.AnimationSmoothness.Close.ClamshellMode",
        "Apps.AppListHide.InputLatency",
    };
    if (MeasureShowLatency())
      names.push_back("Apps.AppListShow.InputLatency");
    return names;
  }

 protected:
  virtual std::string GetAnimationSmoothnessMetricsName() const = 0;

  virtual bool MeasureShowLatency() const { return false; }

  void SendKeyAndWaitForState(ui::KeyboardCode key_code,
                              bool shift_key,
                              ash::AppListViewState target_state) {
    // Browser window is used to identify display, so we can use
    // use the 1st browser window regardless of number of windows created.
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
    ash::ShellTestApi shell_test_api;
    ui_controls::SendKeyPress(browser_window, key_code, /*control=*/false,
                              /*shift=*/shift_key, /*alt=*/false,
                              /*command=*/false);
    shell_test_api.WaitForLauncherAnimationState(target_state);
  }

  // Create the cached widget of the app-list prior to the actual test scenario.
  void CreateCachedWidget() {
    // Here goes through several states of the app-list so that the cached
    // widget can contain relevant data.

    // Open the app-list with peeking state.
    SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, false,
                           ash::AppListViewState::kPeeking);

    // Expand to the fullscreen with list of applications.
    SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, true,
                           ash::AppListViewState::kFullscreenAllApps);

    // Type a random query to switch to search result view.
    SendKeyAndWaitForState(ui::VKEY_X, false,
                           ash::AppListViewState::kFullscreenSearch);

    // Close.
    SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, false,
                           ash::AppListViewState::kClosed);

    // Takes the snapshot delta; so that the samples created so far will be
    // eliminated from the samples.
    for (const auto& name : GetUMAHistogramNames()) {
      auto* histogram = base::StatisticsRecorder::FindHistogram(name);
      if (!histogram)
        continue;
      histogram->SnapshotDelta();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherAnimationsTestBase);
};

class LauncherAnimationsFullscreenTest : public LauncherAnimationsTestBase {
 public:
  LauncherAnimationsFullscreenTest() = default;
  ~LauncherAnimationsFullscreenTest() override = default;

 private:
  // LauncherAnimationsTestBase:
  std::string GetAnimationSmoothnessMetricsName() const override {
    return "FullscreenAllApps.ClamshellMode";
  }
  bool MeasureShowLatency() const override { return true; }

  DISALLOW_COPY_AND_ASSIGN(LauncherAnimationsFullscreenTest);
};

IN_PROC_BROWSER_TEST_P(LauncherAnimationsFullscreenTest, Run) {
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, true,
                         ash::AppListViewState::kFullscreenAllApps);
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, true,
                         ash::AppListViewState::kClosed);
}

INSTANTIATE_TEST_SUITE_P(LauncherAnimations,
                         LauncherAnimationsFullscreenTest,
                         /*reuse_widget=*/::testing::Bool());

class LauncherAnimationsExpandToFullscreenTest
    : public LauncherAnimationsTestBase {
 public:
  LauncherAnimationsExpandToFullscreenTest() = default;
  ~LauncherAnimationsExpandToFullscreenTest() override = default;

 private:
  // LauncherAnimationsTestBase:
  std::string GetAnimationSmoothnessMetricsName() const override {
    return "FullscreenAllApps.ClamshellMode";
  }

  DISALLOW_COPY_AND_ASSIGN(LauncherAnimationsExpandToFullscreenTest);
};

IN_PROC_BROWSER_TEST_P(LauncherAnimationsExpandToFullscreenTest, Run) {
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, false,
                         ash::AppListViewState::kPeeking);
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, true,
                         ash::AppListViewState::kFullscreenAllApps);
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, true,
                         ash::AppListViewState::kClosed);
}

INSTANTIATE_TEST_SUITE_P(LauncherAnimations,
                         LauncherAnimationsExpandToFullscreenTest,
                         /*reuse_widget=*/::testing::Bool());

class LauncherAnimationsPeekingTest : public LauncherAnimationsTestBase {
 public:
  LauncherAnimationsPeekingTest() = default;
  ~LauncherAnimationsPeekingTest() override = default;

 private:
  // LauncherAnimationsTestBase:
  std::string GetAnimationSmoothnessMetricsName() const override {
    return "Peeking.ClamshellMode";
  }
  bool MeasureShowLatency() const override { return true; }

  DISALLOW_COPY_AND_ASSIGN(LauncherAnimationsPeekingTest);
};

IN_PROC_BROWSER_TEST_P(LauncherAnimationsPeekingTest, Run) {
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, false,
                         ash::AppListViewState::kPeeking);
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, false,
                         ash::AppListViewState::kClosed);
}

INSTANTIATE_TEST_SUITE_P(LauncherAnimations,
                         LauncherAnimationsPeekingTest,
                         /*reuse_widget=*/::testing::Bool());

class LauncherAnimationsHalfTest : public LauncherAnimationsTestBase {
 public:
  LauncherAnimationsHalfTest() = default;
  ~LauncherAnimationsHalfTest() override = default;

 private:
  // LauncherAnimationsTestBase:
  std::string GetAnimationSmoothnessMetricsName() const override {
    return "Half.ClamshellMode";
  }

  DISALLOW_COPY_AND_ASSIGN(LauncherAnimationsHalfTest);
};

IN_PROC_BROWSER_TEST_P(LauncherAnimationsHalfTest, Run) {
  // Hit the search key; it should switch to kPeeking state.
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, false,
                         ash::AppListViewState::kPeeking);

  // Type some query in the launcher; it should show search results in kHalf
  // state.
  SendKeyAndWaitForState(ui::VKEY_A, false, ash::AppListViewState::kHalf);

  // Search key to close the launcher.
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, false,
                         ash::AppListViewState::kClosed);
}

INSTANTIATE_TEST_SUITE_P(LauncherAnimations,
                         LauncherAnimationsHalfTest,
                         /*reuse_widget=*/::testing::Bool());

class LauncherAnimationsFullscreenSearchTest
    : public LauncherAnimationsTestBase {
 public:
  LauncherAnimationsFullscreenSearchTest() = default;
  ~LauncherAnimationsFullscreenSearchTest() override = default;

 private:
  // LauncherAnimationsTestBase:
  std::string GetAnimationSmoothnessMetricsName() const override {
    return "FullscreenSearch.ClamshellMode";
  }

  DISALLOW_COPY_AND_ASSIGN(LauncherAnimationsFullscreenSearchTest);
};

IN_PROC_BROWSER_TEST_P(LauncherAnimationsFullscreenSearchTest, Run) {
  // Hit the search key; it should switch to kPeeking state.
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, false,
                         ash::AppListViewState::kPeeking);

  // Type some query; it should show the search results in the kHalf state.
  SendKeyAndWaitForState(ui::VKEY_A, false, ash::AppListViewState::kHalf);

  // Shift+search key; it should expand to fullscreen with search results
  // (i.e. kFullscreenSearch state).
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, true,
                         ash::AppListViewState::kFullscreenSearch);

  // Search key to close the launcher.
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, false,
                         ash::AppListViewState::kClosed);
}

IN_PROC_BROWSER_TEST_P(LauncherAnimationsFullscreenSearchTest,
                       SearchAfterFullscreen) {
  // Hit shift+search key; it should switch to kPeeking state.
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, true,
                         ash::AppListViewState::kFullscreenAllApps);

  // Type some query; it should show the search results in fullscreen (i.e.
  // switching to kFullscreenSearch state).
  SendKeyAndWaitForState(ui::VKEY_A, false,
                         ash::AppListViewState::kFullscreenSearch);

  // Search key to close the launcher.
  SendKeyAndWaitForState(ui::VKEY_BROWSER_SEARCH, false,
                         ash::AppListViewState::kClosed);
}

INSTANTIATE_TEST_SUITE_P(LauncherAnimations,
                         LauncherAnimationsFullscreenSearchTest,
                         /*reuse_widget=*/::testing::Bool());
