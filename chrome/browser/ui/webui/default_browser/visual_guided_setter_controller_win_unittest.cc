// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_controller_win.h"

#include <windows.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/ui/webui/default_browser/settings_window_finder_win.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace {

class TestSettingsWindowFinderWin : public SettingsWindowFinderWin {
 public:
  TestSettingsWindowFinderWin() = default;

  TestSettingsWindowFinderWin(const TestSettingsWindowFinderWin&) = delete;
  TestSettingsWindowFinderWin& operator=(const TestSettingsWindowFinderWin&) =
      delete;

  ~TestSettingsWindowFinderWin() override = default;

  void Start(base::TimeDelta timeout,
             WindowFoundCallback on_found,
             base::OnceClosure on_timeout) override {
    timeout_ = timeout;
    on_found_ = std::move(on_found);
    on_timeout_ = std::move(on_timeout);
    ++start_called_count_;
  }

  void Stop() override {
    ++stop_called_count_;
    on_found_.Reset();
    on_timeout_.Reset();
  }

  void StartObservingLocationChanges(
      HWND settings_hwnd,
      WindowResizedCallback on_resized) override {
    observed_hwnd_ = settings_hwnd;
    on_resized_ = std::move(on_resized);
    ++start_observing_called_count_;
  }

  void StopObservingLocationChanges() override {
    ++stop_observing_called_count_;
    observed_hwnd_ = nullptr;
    on_resized_.Reset();
  }

  void TriggerFound(HWND hwnd) {
    if (hwnd) {
      if (on_found_) {
        std::move(on_found_).Run(hwnd);
      }
    } else {
      if (on_timeout_) {
        std::move(on_timeout_).Run();
      }
    }
  }

  void TriggerResized() {
    if (on_resized_) {
      on_resized_.Run();
    }
  }

  int start_called_count() const { return start_called_count_; }
  int stop_called_count() const { return stop_called_count_; }
  int start_observing_called_count() const {
    return start_observing_called_count_;
  }
  int stop_observing_called_count() const {
    return stop_observing_called_count_;
  }
  base::TimeDelta timeout() const { return timeout_; }

 private:
  WindowFoundCallback on_found_;
  base::OnceClosure on_timeout_;
  WindowResizedCallback on_resized_;
  HWND observed_hwnd_ = nullptr;
  base::TimeDelta timeout_;
  int start_called_count_ = 0;
  int stop_called_count_ = 0;
  int start_observing_called_count_ = 0;
  int stop_observing_called_count_ = 0;
};

class TestVisualGuidedSetterControllerWin
    : public VisualGuidedSetterControllerWin {
 public:
  explicit TestVisualGuidedSetterControllerWin(views::Widget* parent_widget)
      : VisualGuidedSetterControllerWin(parent_widget) {}

  void SetAnchorRect(const std::optional<gfx::Rect>& anchor_rect) {
    anchor_rect_ = anchor_rect;
  }

  void SetSettingsWindowValid(bool valid) { settings_window_valid_ = valid; }
  void SetSettingsWindowClosed(bool closed) {
    settings_window_closed_ = closed;
  }
  void SetChromeWindowActive(bool active) { chrome_window_active_ = active; }
  void SetDpiCompatible(bool compatible) { dpi_compatible_ = compatible; }

  const std::vector<gfx::Rect>& applied_rects() const { return applied_rects_; }
  void clear_applied_rects() { applied_rects_.clear(); }

  const std::vector<HWND>& applied_z_orders() const {
    return applied_z_orders_;
  }
  void clear_applied_z_orders() { applied_z_orders_.clear(); }

  void set_run_loop_quit_closure(base::OnceClosure closure) {
    run_loop_quit_closure_ = std::move(closure);
  }

  TestSettingsWindowFinderWin* test_finder() const { return test_finder_; }

  // VisualGuidedSetterControllerWin:
  bool IsSettingsWindowAlive() const override {
    return settings_hwnd_for_testing() != nullptr;
  }
  bool IsSettingsWindowValid() const override {
    return settings_window_valid_ && IsSettingsWindowAlive();
  }
  bool IsSettingsWindowClosed() const override {
    return settings_window_closed_;
  }

  std::optional<gfx::Rect> GetAnchorRectScreen() const override {
    return anchor_rect_;
  }

  void ApplySettingsRectAndZOrder(const gfx::Rect& target_rect,
                                  HWND insert_after) override {
    applied_rects_.push_back(target_rect);
    applied_z_orders_.push_back(insert_after);
    if (run_loop_quit_closure_) {
      std::move(run_loop_quit_closure_).Run();
    }
  }

  bool IsChromeWindowActive() const override { return chrome_window_active_; }
  void LaunchSettings() override {}

  std::unique_ptr<SettingsWindowFinderWin> CreateSettingsWindowFinder()
      override {
    auto finder = std::make_unique<TestSettingsWindowFinderWin>();
    test_finder_ = finder.get();
    return finder;
  }

  bool IsDpiCompatibleForDocking(HWND hwnd,
                                 const gfx::Rect& target_rect) const override {
    return dpi_compatible_;
  }

 private:
  std::optional<gfx::Rect> anchor_rect_;

  bool settings_window_valid_ = true;
  bool settings_window_closed_ = false;
  bool chrome_window_active_ = true;
  bool dpi_compatible_ = true;
  std::vector<gfx::Rect> applied_rects_;
  std::vector<HWND> applied_z_orders_;
  base::OnceClosure run_loop_quit_closure_;
  mutable raw_ptr<TestSettingsWindowFinderWin> test_finder_ = nullptr;
};

}  // namespace

class VisualGuidedSetterControllerWinTest : public ChromeViewsTestBase {
 protected:
  VisualGuidedSetterControllerWinTest() {
    scoped_feature_list_.InitAndEnableFeature(
        default_browser::kVisualGuidedSetterDocking);
  }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();

    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    content::WebContentsTester::For(web_contents_.get())
        ->SetLastCommittedURL(GURL("chrome://default-browser"));

    controller_ =
        std::make_unique<TestVisualGuidedSetterControllerWin>(widget_.get());
    controller_->SetWebContents(web_contents_.get());

    gfx::Rect anchor(400, 300, 600, 400);
    controller_->SetAnchorRect(anchor);
    controller_->SetAnchorRectInWebUi(gfx::Rect(0, 0, 600, 400));
  }

  void TearDown() override {
    controller_.reset();
    web_contents_.reset();
    profile_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<TestVisualGuidedSetterControllerWin> controller_;
};

TEST_F(VisualGuidedSetterControllerWinTest, StartFindsSettingsWindow) {
  base::HistogramTester histograms;
  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();

  EXPECT_TRUE(controller_->is_running());
  EXPECT_EQ(controller_->test_finder()->start_called_count(), 1);

  // Simulate finding the window through the mock finder.
  base::RunLoop run_loop;
  controller_->set_run_loop_quit_closure(run_loop.QuitClosure());
  controller_->test_finder()->TriggerFound(fake_hwnd);
  run_loop.Run();

  EXPECT_GT(controller_->applied_rects().size(), 0u);

  controller_->Stop();
  histograms.ExpectUniqueSample(
      "DefaultBrowser.VisualGuide.Outcome",
      TestVisualGuidedSetterControllerWin::Outcome::kSuccess, 1);
}

TEST_F(VisualGuidedSetterControllerWinTest, FindSettingsTimeout) {
  base::HistogramTester histograms;

  controller_->Start();

  EXPECT_EQ(controller_->test_finder()->start_called_count(), 1);

  // Simulate finder timing out (calls back with nullptr).
  controller_->test_finder()->TriggerFound(nullptr);

  histograms.ExpectUniqueSample(
      "DefaultBrowser.VisualGuide.Outcome",
      TestVisualGuidedSetterControllerWin::Outcome::kSettingsWindowNotFound, 1);
}

TEST_F(VisualGuidedSetterControllerWinTest, DpiMismatchDegrades) {
  base::HistogramTester histograms;
  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();
  controller_->test_finder()->TriggerFound(fake_hwnd);
  task_environment()->FastForwardBy(base::Milliseconds(100));

  // DPI compatible initially.
  EXPECT_GT(controller_->applied_rects().size(), 0u);
  controller_->clear_applied_rects();

  // DPI mismatch triggers on next dock tick.
  controller_->SetDpiCompatible(false);
  task_environment()->FastForwardBy(base::Milliseconds(100));

  // Degradation should occur, meaning it stops applying positioning.
  EXPECT_EQ(controller_->applied_rects().size(), 0u);

  controller_->Stop();
  histograms.ExpectUniqueSample(
      "DefaultBrowser.VisualGuide.Outcome",
      TestVisualGuidedSetterControllerWin::Outcome::kDpiMismatch, 1);
}

TEST_F(VisualGuidedSetterControllerWinTest, StageTooSmallDegrades) {
  base::HistogramTester histograms;
  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();
  controller_->test_finder()->TriggerFound(fake_hwnd);
  task_environment()->FastForwardBy(base::Milliseconds(100));

  // Stage too small triggers on next dock tick.
  gfx::Rect small_anchor(100, 100, 200, 100);  // 200x100 is too small.
  controller_->SetAnchorRect(small_anchor);
  task_environment()->FastForwardBy(base::Milliseconds(100));

  controller_->Stop();
  histograms.ExpectUniqueSample(
      "DefaultBrowser.VisualGuide.Outcome",
      TestVisualGuidedSetterControllerWin::Outcome::kStageTooSmall, 1);
}

TEST_F(VisualGuidedSetterControllerWinTest, TopmostPolicyRequiresFocus) {
  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();
  controller_->test_finder()->TriggerFound(fake_hwnd);
  task_environment()->FastForwardBy(base::Milliseconds(100));

  // Default policy is kRequiresFocus, and Chrome is active initially.
  // The Settings window should be positioned as TOPMOST.
  ASSERT_GT(controller_->applied_z_orders().size(), 0u);
  EXPECT_EQ(controller_->applied_z_orders().back(), HWND_TOPMOST);
  controller_->clear_applied_z_orders();

  // If Chrome loses focus, the next tick should apply NOTOPMOST.
  controller_->SetChromeWindowActive(false);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  ASSERT_GT(controller_->applied_z_orders().size(), 0u);
  EXPECT_EQ(controller_->applied_z_orders().back(), HWND_NOTOPMOST);
  controller_->clear_applied_z_orders();

  // If Chrome regains focus, it should go back to TOPMOST.
  controller_->SetChromeWindowActive(true);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  ASSERT_GT(controller_->applied_z_orders().size(), 0u);
  EXPECT_EQ(controller_->applied_z_orders().back(), HWND_TOPMOST);
  controller_->clear_applied_z_orders();

  // Switch policy to kAlways. Even when Chrome is inactive, it should stay
  // TOPMOST.
  controller_->SetTopmostPolicy(
      TestVisualGuidedSetterControllerWin::TopmostPolicy::kAlways);
  controller_->SetChromeWindowActive(false);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  ASSERT_GT(controller_->applied_z_orders().size(), 0u);
  EXPECT_EQ(HWND_TOPMOST, controller_->applied_z_orders().back());

  controller_->Stop();
}

TEST_F(VisualGuidedSetterControllerWinTest,
       RepeatedStartWithoutAnchorDoesNotCrash) {
  // Starting without an anchor rect is now supported; it should start running.
  controller_->SetAnchorRectInWebUi(gfx::Rect());
  controller_->Start();
  EXPECT_TRUE(controller_->is_running());

  // A duplicate Start() call should return early without starting the window
  // finder again.
  controller_->Start();
  EXPECT_TRUE(controller_->is_running());
  EXPECT_EQ(controller_->test_finder()->start_called_count(), 1);
}

TEST_F(VisualGuidedSetterControllerWinTest, StartWithEmptyAnchorRectDegrades) {
  base::HistogramTester histograms;
  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->SetAnchorRectInWebUi(gfx::Rect());
  controller_->SetAnchorRect(std::nullopt);
  controller_->Start();

  EXPECT_TRUE(controller_->is_running());
  controller_->test_finder()->TriggerFound(fake_hwnd);
  task_environment()->FastForwardBy(base::Milliseconds(100));

  // The visual guide should have entered degraded floating mode.
  controller_->Stop();
  histograms.ExpectUniqueSample(
      "DefaultBrowser.VisualGuide.Outcome",
      TestVisualGuidedSetterControllerWin::Outcome::kStageTooSmall, 1);
}

TEST_F(VisualGuidedSetterControllerWinTest, SettingsWindowClosedRecordsError) {
  base::HistogramTester histograms;
  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();
  controller_->test_finder()->TriggerFound(fake_hwnd);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(controller_->is_running());

  // Simulate user closing external Settings window.
  controller_->SetSettingsWindowClosed(true);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_FALSE(controller_->is_running());

  histograms.ExpectUniqueSample(
      "DefaultBrowser.VisualGuide.Outcome",
      TestVisualGuidedSetterControllerWin::Outcome::kSettingsWindowClosed, 1);
}

TEST_F(VisualGuidedSetterControllerWinTest, ContinuousDockingDisabled) {
  // Disable the continuous docking feature for this test.
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      default_browser::kVisualGuidedSetterDocking);

  // We must recreate the controller so that it reads the new feature state
  // at construction.
  controller_ =
      std::make_unique<TestVisualGuidedSetterControllerWin>(widget_.get());
  controller_->SetWebContents(web_contents_.get());
  gfx::Rect anchor(400, 300, 600, 400);
  controller_->SetAnchorRect(anchor);
  controller_->SetAnchorRectInWebUi(gfx::Rect(0, 0, 600, 400));

  base::HistogramTester histograms;
  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();

  EXPECT_TRUE(controller_->is_running());
  EXPECT_EQ(controller_->test_finder()->start_called_count(), 1);

  // Simulate finding the window.
  controller_->test_finder()->TriggerFound(fake_hwnd);

  // The initial docking operation should apply.
  EXPECT_GT(controller_->applied_rects().size(), 0u);
  controller_->clear_applied_rects();
  controller_->clear_applied_z_orders();

  // Fast-forwarding time should NOT run the timer.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  // No subsequent positioning should be applied because timers are not started.
  EXPECT_EQ(controller_->applied_rects().size(), 0u);

  // Simulate window changes (e.g. Chrome loses focus).
  controller_->SetChromeWindowActive(false);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  // No Z-order changes should be applied.
  EXPECT_EQ(controller_->applied_z_orders().size(), 0u);

  controller_->Stop();
}

TEST_F(VisualGuidedSetterControllerWinTest,
       ObservesLocationChangesIfContinuousDockingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      default_browser::kVisualGuidedSetterDocking);

  // We must recreate the controller so that it reads the new feature state
  // at construction.
  controller_ =
      std::make_unique<TestVisualGuidedSetterControllerWin>(widget_.get());
  controller_->SetWebContents(web_contents_.get());
  gfx::Rect anchor(400, 300, 600, 400);
  controller_->SetAnchorRect(anchor);
  controller_->SetAnchorRectInWebUi(gfx::Rect(0, 0, 600, 400));

  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();
  controller_->test_finder()->TriggerFound(fake_hwnd);

  // Triggering window discovery should call StartObservingLocationChanges.
  EXPECT_EQ(controller_->test_finder()->start_observing_called_count(), 1);

  // Clear previously recorded calls.
  controller_->clear_applied_rects();

  // Simulate a window resize/move event.
  controller_->test_finder()->TriggerResized();

  // Triggering the resize callback should run UpdateDockedLayout, which calls
  // ApplySettingsRectAndZOrder.
  EXPECT_GT(controller_->applied_rects().size(), 0u);

  // Stopping the controller should call StopObservingLocationChanges.
  controller_->Stop();
  EXPECT_EQ(controller_->test_finder()->stop_observing_called_count(), 1);
}

// Verifies that continuous docking layout updates pause when the tab is
// hidden and resume when returning to the tab.
TEST_F(VisualGuidedSetterControllerWinTest,
       OnVisibilityChangedHidesAndRestoresLayout) {
  gfx::Rect anchor(400, 300, 600, 400);
  controller_->SetAnchorRect(anchor);
  controller_->SetAnchorRectInWebUi(gfx::Rect(0, 0, 600, 400));

  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();
  controller_->test_finder()->TriggerFound(fake_hwnd);
  controller_->clear_applied_rects();

  web_contents_->WasHidden();

  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(controller_->applied_rects().size(), 0u);

  web_contents_->WasShown();
  controller_->clear_applied_rects();

  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(controller_->applied_rects().size(), 2u);

  controller_->Stop();
}

// Verifies that location change observation stops when the tab is hidden and
// restarts when returning to the tab (when continuous docking is disabled).
TEST_F(VisualGuidedSetterControllerWinTest,
       OnVisibilityChangedHidesAndRestoresLayoutWhenContinuousDockingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      default_browser::kVisualGuidedSetterDocking);

  controller_ =
      std::make_unique<TestVisualGuidedSetterControllerWin>(widget_.get());
  controller_->SetWebContents(web_contents_.get());
  gfx::Rect anchor(400, 300, 600, 400);
  controller_->SetAnchorRect(anchor);
  controller_->SetAnchorRectInWebUi(gfx::Rect(0, 0, 600, 400));

  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();
  controller_->test_finder()->TriggerFound(fake_hwnd);
  EXPECT_EQ(controller_->test_finder()->start_observing_called_count(), 1);

  web_contents_->WasHidden();
  EXPECT_EQ(controller_->test_finder()->stop_observing_called_count(), 1);

  web_contents_->WasShown();
  EXPECT_EQ(controller_->test_finder()->start_observing_called_count(), 2);

  controller_->Stop();
}

// Verifies that finding the Settings window while the tab is hidden suppresses
// starting docking layout updates.
TEST_F(VisualGuidedSetterControllerWinTest,
       OnSettingsWindowFoundHidesIfTabHidden) {
  gfx::Rect anchor(400, 300, 600, 400);
  controller_->SetAnchorRect(anchor);
  controller_->SetAnchorRectInWebUi(gfx::Rect(0, 0, 600, 400));

  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();
  web_contents_->WasHidden();

  controller_->test_finder()->TriggerFound(fake_hwnd);

  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(controller_->applied_rects().size(), 0u);

  controller_->Stop();
}

// Verifies that navigating away from chrome://default-browser stops the
// controller and tears down docking.
TEST_F(VisualGuidedSetterControllerWinTest,
       PrimaryPageChangedStopsControllerWhenNavigatedAway) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetLastCommittedURL(GURL("https://www.google.com"));

  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();
  controller_->test_finder()->TriggerFound(fake_hwnd);
  EXPECT_TRUE(controller_->is_running());

  controller_->PrimaryPageChanged(web_contents_->GetPrimaryPage());

  EXPECT_FALSE(controller_->is_running());
}

// Verifies that returning to a tab does not resume layout observation if the
// Settings window was closed while hidden.
TEST_F(VisualGuidedSetterControllerWinTest,
       OnVisibilityChangedDoesNotRestoreIfSettingsWindowClosed) {
  gfx::Rect anchor(400, 300, 600, 400);
  controller_->SetAnchorRect(anchor);
  controller_->SetAnchorRectInWebUi(gfx::Rect(0, 0, 600, 400));

  HWND fake_hwnd = reinterpret_cast<HWND>(0x12345);

  controller_->Start();
  controller_->test_finder()->TriggerFound(fake_hwnd);

  web_contents_->WasHidden();
  controller_->SetSettingsWindowValid(false);
  controller_->clear_applied_rects();

  web_contents_->WasShown();

  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(controller_->applied_rects().size(), 0u);

  controller_->Stop();
}
