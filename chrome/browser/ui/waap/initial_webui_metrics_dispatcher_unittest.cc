// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_metrics_dispatcher.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/mock_base_window.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace {

constexpr base::TimeDelta kTestLatency = base::Milliseconds(100);

class InitialWebUIMetricsDispatcherTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    startup_metric_utils::GetBrowser().ResetSessionForTesting();
    InitialWebUIWindowMetricsManager::ResetForTesting();

    EXPECT_CALL(browser_window_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));
    EXPECT_CALL(testing::Const(browser_window_), GetProfile())
        .WillRepeatedly(testing::Return(profile()));
    EXPECT_CALL(browser_window_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
  }

  MockBrowserWindowInterface browser_window_;
  ui::UnownedUserDataHost unowned_user_data_host_;
};

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(InitialWebUIMetricsDispatcherTest, StartupFirstPaintLoggedDirectly) {
  base::HistogramTester histograms;
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  webui::SetBrowserWindowInterface(web_contents(), &browser_window_);
  manager.SetWindowCreationInfo(waap::NewWindowCreationSource::kNewEmptyWindow,
                                base::TimeTicks::Now());

  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(dispatcher);

  base::TimeTicks paint_time = base::TimeTicks::Now();
  dispatcher->OnFirstPaint(paint_time);

  // Directly records startup metric to WaapUIMetricsService.
  histograms.ExpectTotalCount("InitialWebUI.Startup.ReloadButton.FirstPaint",
                              1);
  // Does NOT record new window metric.
  histograms.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstPaint.FromConstructor2",
      0);
}

TEST_F(InitialWebUIMetricsDispatcherTest,
       StartupFirstContentfulPaintLoggedDirectly) {
  base::HistogramTester histograms;
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  webui::SetBrowserWindowInterface(web_contents(), &browser_window_);
  manager.SetWindowCreationInfo(waap::NewWindowCreationSource::kNewEmptyWindow,
                                base::TimeTicks::Now());

  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(dispatcher);

  base::TimeTicks fcp_time = base::TimeTicks::Now();
  dispatcher->OnFirstContentfulPaint(fcp_time);

  // Directly records startup metric to WaapUIMetricsService.
  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);
  // Does NOT record new window metric.
  histograms.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstContentfulPaint.FromConstructor2",
      0);
}

TEST_F(InitialWebUIMetricsDispatcherTest, StartupOnCommitDeferredUntilBound) {
  base::HistogramTester histograms;
  InitialWebUIWindowMetricsManager manager(&browser_window_);

  // Commit before the WebContents is attached to a window (e.g. prewarmed).
  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(dispatcher);

  base::TimeTicks init_time = base::TimeTicks::Now();
  base::TimeTicks launched_time = init_time + base::Milliseconds(50);
  dispatcher->OnCommit(init_time, launched_time);

  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.RendererProcessCreated", 0);
  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.RendererProcessLaunched", 0);

  webui::SetBrowserWindowInterface(web_contents(), &browser_window_);

  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.RendererProcessCreated", 1);
  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.RendererProcessLaunched", 1);
}

// A WebContents in the shared TopChromeWebUIProfile, which has no
// `WaapUIMetricsService`, records startup metrics with the service of the
// profile of its window.
// TODO(crbug.com/548363252): Add a browser test with
// `features::kTopChromeWebUISharedRenderer` that checks these metrics end to
// end, once the WebUI toolbar is created in TopChromeWebUIProfile.
TEST_F(InitialWebUIMetricsDispatcherTest,
       StartupMetricsUseWindowProfileForTopChromeWebUIProfile) {
  webui::ScopedTopChromeWebUIProfileForTesting scoped_top_chrome_webui_profile;
  Profile* top_chrome_webui_profile = webui::GetTopChromeWebUIProfile();
  ASSERT_FALSE(WaapUIMetricsService::Get(top_chrome_webui_profile));
  std::unique_ptr<content::WebContents> contents =
      content::WebContentsTester::CreateTestWebContents(
          top_chrome_webui_profile, /*instance=*/nullptr);

  base::HistogramTester histograms;
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  webui::SetBrowserWindowInterface(contents.get(), &browser_window_);

  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(contents.get());
  ASSERT_TRUE(dispatcher);

  base::TimeTicks start_time = base::TimeTicks::Now();
  dispatcher->OnCommit(start_time, start_time + base::Milliseconds(10));
  dispatcher->OnFirstPaint(start_time + kTestLatency);
  dispatcher->OnFirstContentfulPaint(start_time + kTestLatency);

  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.RendererProcessCreated", 1);
  histograms.ExpectTotalCount("InitialWebUI.Startup.ReloadButton.FirstPaint",
                              1);
  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// A prewarmed WebContents that is destroyed without ever being taken by a
// window must not record the startup renderer process metrics.
TEST_F(InitialWebUIMetricsDispatcherTest, StartupOnCommitDroppedIfNeverBound) {
  base::HistogramTester histograms;
  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(dispatcher);

  base::TimeTicks init_time = base::TimeTicks::Now();
  dispatcher->OnCommit(init_time, init_time + base::Milliseconds(50));
  DeleteContents();

  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.RendererProcessCreated", 0);
  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.RendererProcessLaunched", 0);
}

// Windows shown minimized skip all latency metrics, including startup ones.
TEST_F(InitialWebUIMetricsDispatcherTest, SkipsLatencyMetricsWhenMinimized) {
  base::HistogramTester histograms;
  testing::NiceMock<ui::MockBaseWindow> test_window;
  EXPECT_CALL(test_window, IsMinimized()).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(browser_window_, GetWindow())
      .WillRepeatedly(testing::Return(&test_window));

  InitialWebUIWindowMetricsManager manager(&browser_window_);
  webui::SetBrowserWindowInterface(web_contents(), &browser_window_);
  base::TimeTicks start_time = base::TimeTicks::Now();
  manager.SetWindowCreationInfo(waap::NewWindowCreationSource::kNewEmptyWindow,
                                start_time);
  manager.OnBrowserWindowShowRequested(start_time);
  ASSERT_TRUE(manager.should_skip_latency_metrics());

  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(dispatcher);

  base::TimeTicks paint_time = start_time + kTestLatency;
  dispatcher->OnCommit(start_time, start_time + base::Milliseconds(10));
  dispatcher->OnFirstPaint(paint_time);
  dispatcher->OnFirstContentfulPaint(paint_time);

  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.RendererProcessCreated", 0);
  histograms.ExpectTotalCount("InitialWebUI.Startup.ReloadButton.FirstPaint",
                              0);
  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 0);
  histograms.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstPaint.FromConstructor2",
      0);
  histograms.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstContentfulPaint.FromConstructor2",
      0);
}

// `InitialWebUIWindowMetricsManager::SkipStartupForTesting()` only affects its
// own window: paints are reported as new-window metrics and the process-wide
// startup markers stay untouched for other windows.
TEST_F(InitialWebUIMetricsDispatcherTest, SkipStartupForTestingIsPerWindow) {
  base::HistogramTester histograms;
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  manager.SkipStartupForTesting();
  webui::SetBrowserWindowInterface(web_contents(), &browser_window_);
  base::TimeTicks start_time = base::TimeTicks::Now();
  manager.SetWindowCreationInfo(waap::NewWindowCreationSource::kNewEmptyWindow,
                                start_time);

  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(dispatcher);

  base::TimeTicks paint_time = start_time + kTestLatency;
  dispatcher->OnCommit(start_time, start_time + base::Milliseconds(10));
  dispatcher->OnFirstPaint(paint_time);
  dispatcher->OnFirstContentfulPaint(paint_time);

  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.RendererProcessCreated", 0);
  histograms.ExpectTotalCount("InitialWebUI.Startup.ReloadButton.FirstPaint",
                              0);
  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 0);
  histograms.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstPaint.FromConstructor2",
      kTestLatency, 1);
  histograms.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstContentfulPaint.FromConstructor2",
      kTestLatency, 1);
  EXPECT_FALSE(WaapUIMetricsService::IsStartupFirstPaintRecorded());
  EXPECT_FALSE(WaapUIMetricsService::IsStartupFirstContentfulPaintRecorded());
}

TEST_F(InitialWebUIMetricsDispatcherTest, NewWindowFirstPaintBoundAtStart) {
  base::HistogramTester histograms;
  WaapUIMetricsService::SkipStartupForTesting();

  InitialWebUIWindowMetricsManager manager(&browser_window_);
  webui::SetBrowserWindowInterface(web_contents(), &browser_window_);
  base::TimeTicks start_time = base::TimeTicks::Now();
  manager.SetWindowCreationInfo(waap::NewWindowCreationSource::kNewEmptyWindow,
                                start_time);

  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(dispatcher);

  base::TimeTicks paint_time = start_time + kTestLatency;
  dispatcher->OnFirstPaint(paint_time);
  dispatcher->OnFirstContentfulPaint(paint_time);

  histograms.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstPaint.FromConstructor2",
      kTestLatency, 1);
  histograms.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstContentfulPaint.FromConstructor2",
      kTestLatency, 1);
}

// Non-normal windows (e.g. popups) do not have an
// `InitialWebUIWindowMetricsManager` and must not record startup or new-window
// paint metrics.
TEST_F(InitialWebUIMetricsDispatcherTest, PaintsIgnoredWithoutManager) {
  base::HistogramTester histograms;
  webui::SetBrowserWindowInterface(web_contents(), &browser_window_);

  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(dispatcher);

  base::TimeTicks paint_time = base::TimeTicks::Now();
  dispatcher->OnFirstPaint(paint_time);
  dispatcher->OnFirstContentfulPaint(paint_time);

  histograms.ExpectTotalCount("InitialWebUI.Startup.ReloadButton.FirstPaint",
                              0);
  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 0);
  histograms.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstPaint.FromConstructor2",
      0);
  histograms.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstContentfulPaint.FromConstructor2",
      0);
  EXPECT_FALSE(WaapUIMetricsService::IsStartupFirstPaintRecorded());
  EXPECT_FALSE(WaapUIMetricsService::IsStartupFirstContentfulPaintRecorded());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(InitialWebUIMetricsDispatcherTest,
       RepeatedPaintsDoNotEmitNewWindowMetricsForStartupWindow) {
  base::HistogramTester histograms;
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  webui::SetBrowserWindowInterface(web_contents(), &browser_window_);
  base::TimeTicks start_time = base::TimeTicks::Now();
  manager.SetWindowCreationInfo(waap::NewWindowCreationSource::kNewEmptyWindow,
                                start_time);

  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(dispatcher);

  base::TimeTicks paint_time1 = start_time + kTestLatency;
  base::TimeTicks paint_time2 = paint_time1 + kTestLatency;
  dispatcher->OnFirstPaint(paint_time1);
  dispatcher->OnFirstPaint(paint_time2);
  dispatcher->OnFirstContentfulPaint(paint_time1);
  dispatcher->OnFirstContentfulPaint(paint_time2);

  histograms.ExpectTotalCount("InitialWebUI.Startup.ReloadButton.FirstPaint",
                              1);
  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);
  // Startup window must not also emit NewWindow metrics even after a second
  // paint call.
  histograms.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstPaint.FromConstructor2",
      0);
  histograms.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstContentfulPaint.FromConstructor2",
      0);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(InitialWebUIMetricsDispatcherTest, NativeDispatcherLogsStartupMetrics) {
  base::HistogramTester histograms;
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  auto* dispatcher = manager.GetMetricsDispatcher();
  ASSERT_TRUE(dispatcher);
  dispatcher->OnCreated();
  histograms.ExpectBucketCount("InitialWebUI.View.Creation", 1, 1);

  base::TimeTicks paint_time = base::TimeTicks::Now();
  dispatcher->OnFirstPaint(paint_time);
  dispatcher->OnFirstContentfulPaint(paint_time);

  histograms.ExpectTotalCount("InitialWebUI.Startup.ReloadButton.FirstPaint",
                              1);
  histograms.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(InitialWebUIMetricsDispatcherTest,
       NativeDispatcherLogsNewWindowMetrics) {
  base::HistogramTester histograms;
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  // The native dispatcher is always bound, so the per-window skip applies.
  manager.SkipStartupForTesting();
  base::TimeTicks start_time = base::TimeTicks::Now();
  manager.SetWindowCreationInfo(waap::NewWindowCreationSource::kNewEmptyWindow,
                                start_time);

  auto* dispatcher = manager.GetMetricsDispatcher();
  ASSERT_TRUE(dispatcher);
  base::TimeTicks paint_time = start_time + kTestLatency;
  dispatcher->OnFirstPaint(paint_time);
  dispatcher->OnFirstContentfulPaint(paint_time);

  histograms.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstPaint.FromConstructor2",
      kTestLatency, 1);
  histograms.ExpectUniqueTimeSample(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "ReloadButton.FirstContentfulPaint.FromConstructor2",
      kTestLatency, 1);
}

// Creation of a WebUI surface is recorded once its WebContents is attached to
// a window that has an InitialWebUIWindowMetricsManager.
TEST_F(InitialWebUIMetricsDispatcherTest,
       WebContentsDispatcherOnCreatedRecordsWithManager) {
  base::HistogramTester histograms;
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  webui::SetBrowserWindowInterface(web_contents(), &browser_window_);

  InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents())
      ->OnCreated();

  histograms.ExpectUniqueSample("InitialWebUI.View.Creation", 1, 1);
}

// Creation is not recorded for a WebContents whose window does not track
// InitialWebUI metrics (e.g. not attached, or a popup without a manager).
TEST_F(InitialWebUIMetricsDispatcherTest,
       WebContentsDispatcherOnCreatedSkippedWithoutManager) {
  base::HistogramTester histograms;

  InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents())
      ->OnCreated();

  histograms.ExpectTotalCount("InitialWebUI.View.Creation", 0);
}

}  // namespace
