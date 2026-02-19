// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/metrics_reporter/mock_metrics_reporter.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::InvokeArgument;
using ::testing::Return;

// Measurement marks.
constexpr char kChangeVisibleModeToLoadingStartMark[] =
    "BrowserControls.ChangeVisibleModeToLoading.Start";
constexpr char kChangeVisibleModeToNotLoadingStartMark[] =
    "BrowserControls.ChangeVisibleModeToNotLoading.Start";
constexpr char kInputMouseReleaseStartMark[] =
    "ReloadButton.Input.MouseRelease.Start";

// Histogram names.
constexpr char kInputToReloadMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToReload.MouseRelease";
constexpr char kInputToStopMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToStop.MouseRelease";

}  // namespace

// Test fixture for the BrowserControlsService class.
class BrowserControlsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    auto* service =
        MetricsReporterService::GetFromWebContents(web_contents_.get());
    auto mock_metrics_reporter =
        std::make_unique<testing::NiceMock<MockMetricsReporter>>();
    mock_metrics_reporter_ = mock_metrics_reporter.get();
    service->SetMetricsReporterForTesting(std::move(mock_metrics_reporter));

    mock_command_updater_ =
        std::make_unique<testing::NiceMock<MockCommandUpdater>>();

    ON_CALL(mock_browser_window_, GetProfile())
        .WillByDefault(Return(&profile_));
    ON_CALL(mock_browser_window_, GetTabStripModel())
        .WillByDefault(Return(nullptr));

    ON_CALL(delegate_, GetNavigationControlsState()).WillByDefault([]() {
      return CreateValidNavigationControlsState();
    });

    handler_ = std::make_unique<BrowserControlsService>(
        mojo::PendingReceiver<
            browser_controls_api::mojom::BrowserControlsService>(),
        web_contents_.get(), mock_command_updater_.get(), &mock_browser_window_,
        /*delegate=*/&delegate_);

    base::RunLoop run_loop;
    handler_->Bind(base::BindLambdaForTesting(
        [&](base::expected<browser_controls_api::mojom::InitialStatePtr,
                           mojo_base::mojom::ErrorPtr> result) {
          ASSERT_TRUE(result.has_value());
          page().Bind(std::move(result.value()->update_stream));
          run_loop.Quit();
        }));
    run_loop.Run();

    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
  }

  void TearDown() override { handler_.reset(); }

 protected:
  void ExpectMeasureAndClearMark(const std::string& start_mark,
                                 base::TimeDelta duration) {
    EXPECT_CALL(mock_metrics_reporter(),
                Measure(Eq(start_mark), testing::A<base::TimeTicks>(), _))
        .WillOnce(base::test::RunOnceCallback<2>(duration));
    EXPECT_CALL(mock_metrics_reporter(), ClearMark(start_mark));
  }

  void ExpectNoMeasureCallback(const std::string& start_mark) {
    EXPECT_CALL(mock_metrics_reporter(),
                Measure(Eq(start_mark), testing::A<base::TimeTicks>(), _))
        .Times(1);
    // OnMeasureResultAndClearMark() calls ClearMark(). Expecting ClearMark() to
    // not be called ensures that the callback is not triggered.
    EXPECT_CALL(mock_metrics_reporter(), ClearMark(Eq(start_mark))).Times(0);
  }

  MetricsReporterService* GetService() {
    return MetricsReporterService::GetFromWebContents(web_contents_.get());
  }

  void ClearMetricsReporter() {
    // Must be clear before setting the service to null.
    mock_metrics_reporter_ = nullptr;
    GetService()->SetMetricsReporterForTesting(nullptr);
  }

  testing::StrictMock<MockReloadButtonPage>& page() { return page_; }
  content::WebContents& web_contents() { return *web_contents_; }
  TestingProfile& profile() { return profile_; }
  testing::NiceMock<MockCommandUpdater>& mock_command_updater() {
    return *mock_command_updater_;
  }
  testing::NiceMock<MockMetricsReporter>& mock_metrics_reporter() {
    return *mock_metrics_reporter_;
  }

  BrowserControlsService& handler() { return *handler_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  MockWebWebUIToolbarDelegate& delegate() { return delegate_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  testing::StrictMock<MockReloadButtonPage> page_;
  std::unique_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_;
  std::unique_ptr<testing::NiceMock<MockCommandUpdater>> mock_command_updater_;
  raw_ptr<testing::NiceMock<MockMetricsReporter>> mock_metrics_reporter_;
  std::unique_ptr<BrowserControlsService> handler_;
  base::HistogramTester histogram_tester_;
  MockWebWebUIToolbarDelegate delegate_;
};

// Test suite for Reload-related tests.
using BrowserControlsServiceReloadTest = BrowserControlsServiceTest;

// Tests that calling Reload(false, {}) executes the IDC_RELOAD command and
// records metrics.
TEST_F(BrowserControlsServiceReloadTest, ReloadByMouseRelease) {
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_RELOAD, WindowOpenDisposition::CURRENT_TAB, testing::_));
  EXPECT_CALL(delegate(), PermitLaunchUrl()).Times(1);

  const base::TimeDelta duration = base::Milliseconds(10);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  handler().ReloadFromClick(/*bypass_cache=*/false, /*click_flags=*/{});

  histogram_tester().ExpectUniqueTimeSample(kInputToReloadMouseReleaseHistogram,
                                            duration, 1);
}

// Tests that calling Reload(false, {}) doesn't record metrics if the start mark
// is not present.
TEST_F(BrowserControlsServiceReloadTest, ReloadByMouseReleaseNoStartMark) {
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_RELOAD, WindowOpenDisposition::CURRENT_TAB, testing::_));
  ExpectNoMeasureCallback(kInputMouseReleaseStartMark);

  handler().ReloadFromClick(/*bypass_cache=*/false, /*click_flags=*/{});

  histogram_tester().ExpectTotalCount(kInputToReloadMouseReleaseHistogram, 0);
}

// Tests that calling Reload(false, {middle_button}) executes the
// IDC_RELOAD with new background tab.
TEST_F(BrowserControlsServiceReloadTest, ReloadWithMiddleMouseButton) {
  EXPECT_CALL(
      mock_command_updater(),
      ExecuteCommandWithDisposition(
          IDC_RELOAD, WindowOpenDisposition::NEW_BACKGROUND_TAB, testing::_));

  const base::TimeDelta duration = base::Milliseconds(10);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  handler().ReloadFromClick(
      /*bypass_cache=*/false,
      /*click_flags=*/{browser_controls_api::mojom::ClickDispositionFlag::
                           kMiddleMouseButton});

  histogram_tester().ExpectUniqueTimeSample(kInputToReloadMouseReleaseHistogram,
                                            duration, 1);
}

// Tests that calling Reload(false, {}) does not crash if the metrics reporter
// is null.
TEST_F(BrowserControlsServiceReloadTest, ReloadNoMetricsReporter) {
  // Reset the metrics reporter to null.
  ClearMetricsReporter();

  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_RELOAD, WindowOpenDisposition::CURRENT_TAB, testing::_));
  // No EXPECT_CALLs for mock_metrics_reporter_ as it is null.

  handler().ReloadFromClick(/*bypass_cache=*/false, /*click_flags=*/{});
  // Expect no crash.
}

// Tests that calling Reload(true) executes the IDC_RELOAD_BYPASSING_CACHE
TEST_F(BrowserControlsServiceReloadTest, ReloadBypassingCache) {
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(IDC_RELOAD_BYPASSING_CACHE,
                                            WindowOpenDisposition::CURRENT_TAB,
                                            testing::_))
      .Times(1);

  handler().ReloadFromClick(/*bypass_cache=*/true, /*click_flags=*/{});
}

// Test suite for StopLoad-related tests.
using BrowserControlsServiceStopLoadTest = BrowserControlsServiceTest;

// Tests that calling StopLoad() executes the IDC_STOP command and records
// metrics.
TEST_F(BrowserControlsServiceStopLoadTest, StopLoad) {
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_STOP, WindowOpenDisposition::CURRENT_TAB, testing::_));
  const base::TimeDelta duration = base::Milliseconds(20);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  handler().StopLoad();

  histogram_tester().ExpectUniqueTimeSample(kInputToStopMouseReleaseHistogram,
                                            duration, 1);
}

// Tests that calling StopLoad() doesn't record metrics if the start mark
// is not present.
TEST_F(BrowserControlsServiceStopLoadTest, StopLoadNoStartMark) {
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_STOP, WindowOpenDisposition::CURRENT_TAB, testing::_));
  ExpectNoMeasureCallback(kInputMouseReleaseStartMark);

  handler().StopLoad();

  histogram_tester().ExpectTotalCount(kInputToStopMouseReleaseHistogram, 0);
}

// Tests that calling StopLoad() does not crash if the metrics reporter is
// null.
TEST_F(BrowserControlsServiceStopLoadTest, StopLoadNoMetricsReporter) {
  ClearMetricsReporter();
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_STOP, WindowOpenDisposition::CURRENT_TAB, testing::_));
  // No EXPECT_CALLs for `mock_metrics_reporter()` as it is null.

  handler().StopLoad();
  // Expect no crash.
}

// Tests that calling ShowContextMenu() opens the context menu.
TEST_F(BrowserControlsServiceTest, TestShowContextMenu) {
  EXPECT_CALL(delegate(),
              HandleContextMenu(testing::_, testing::_, testing::_));

  handler().ShowContextMenu(
      browser_controls_api::mojom::ContextMenuType::kReload, gfx::Point(1, 2),
      ui::mojom::MenuSourceType::kMouse);
  web_contents().SetDelegate(nullptr);
}

// Tests that calling OnNavigationControlsStateChanged() calls the page with the
// correct state and records metrics when loading.
TEST_F(BrowserControlsServiceTest, TestOnNavigationStatusChangedLoading) {
  auto state = CreateValidNavigationControlsState();
  state->reload_control_state->is_navigation_loading = true;

  EXPECT_CALL(page(),
              OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
                  &browser_controls_api::mojom::NavigationControlsState::
                      reload_control_state,
                  testing::Pointee(testing::Field(
                      &browser_controls_api::mojom::ReloadControlState::
                          is_navigation_loading,
                      true))))))
      .Times(1);
  EXPECT_CALL(mock_metrics_reporter(),
              Mark(kChangeVisibleModeToLoadingStartMark))
      .Times(1);

  handler().OnNavigationControlsStateChanged(std::move(state));

  page().FlushForTesting();
}

// Tests that calling OnNavigationControlsStateChanged() calls the page with the
// correct state and records metrics when not loading.
TEST_F(BrowserControlsServiceTest, TestOnNavigationStatusChangedNotLoading) {
  auto state = CreateValidNavigationControlsState();
  state->reload_control_state->is_navigation_loading = false;

  EXPECT_CALL(page(),
              OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
                  &browser_controls_api::mojom::NavigationControlsState::
                      reload_control_state,
                  testing::Pointee(testing::Field(
                      &browser_controls_api::mojom::ReloadControlState::
                          is_navigation_loading,
                      false))))))
      .Times(1);
  EXPECT_CALL(mock_metrics_reporter(),
              Mark(kChangeVisibleModeToNotLoadingStartMark))
      .Times(1);
  handler().OnNavigationControlsStateChanged(std::move(state));

  page().FlushForTesting();
}

// Tests that calling OnNavigationControlsStateChanged() calls the page with the
// correct state.
TEST_F(BrowserControlsServiceTest, TestOnDevToolsStatusChangedToConnected) {
  auto state = CreateValidNavigationControlsState();
  state->reload_control_state->is_devtools_connected = true;

  EXPECT_CALL(page(),
              OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
                  &browser_controls_api::mojom::NavigationControlsState::
                      reload_control_state,
                  testing::Pointee(testing::Field(
                      &browser_controls_api::mojom::ReloadControlState::
                          is_devtools_connected,
                      true))))))
      .Times(1);

  handler().OnNavigationControlsStateChanged(std::move(state));

  page().FlushForTesting();
}

// Tests that calling OnNavigationControlsStateChanged() calls the page with the
// correct state.
TEST_F(BrowserControlsServiceTest, TestOnDevToolsStatusChangedToDisconnected) {
  auto state = CreateValidNavigationControlsState();
  state->reload_control_state->is_devtools_connected = false;

  EXPECT_CALL(page(),
              OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
                  &browser_controls_api::mojom::NavigationControlsState::
                      reload_control_state,
                  testing::Pointee(testing::Field(
                      &browser_controls_api::mojom::ReloadControlState::
                          is_devtools_connected,
                      false))))))
      .Times(1);

  handler().OnNavigationControlsStateChanged(std::move(state));
  page().FlushForTesting();
}

// Tests that calling OnNavigationControlsStateChanged() does not crash if the
// metrics reporter is null.
TEST_F(BrowserControlsServiceTest,
       TestOnNavigationStatusChangedNoMetricsReporter) {
  ClearMetricsReporter();

  auto state = CreateValidNavigationControlsState();
  state->reload_control_state->is_navigation_loading = true;

  EXPECT_CALL(page(),
              OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
                  &browser_controls_api::mojom::NavigationControlsState::
                      reload_control_state,
                  testing::Pointee(testing::Field(
                      &browser_controls_api::mojom::ReloadControlState::
                          is_navigation_loading,
                      true))))))
      .Times(1);
  // No EXPECT_CALLs for `mock_metrics_reporter()` as it is null.

  handler().OnNavigationControlsStateChanged(std::move(state));

  page().FlushForTesting();
  // Expect no crash.
}

// Tests that multiple observers receive updates.
TEST_F(BrowserControlsServiceTest, MultipleObserversReceiveUpdates) {
  testing::StrictMock<MockReloadButtonPage> page2;

  // Bind a second observer.
  base::RunLoop run_loop;
  handler().Bind(base::BindLambdaForTesting(
      [&](base::expected<browser_controls_api::mojom::InitialStatePtr,
                         mojo_base::mojom::ErrorPtr> result) {
        ASSERT_TRUE(result.has_value());
        page2.Bind(std::move(result.value()->update_stream));
        run_loop.Quit();
      }));
  run_loop.Run();

  // Trigger an event.
  EXPECT_CALL(page2, OnNavigationControlsStateChanged(testing::_)).Times(1);

  // The original page should also receive the event.
  EXPECT_CALL(page(), OnNavigationControlsStateChanged(testing::_)).Times(1);

  EXPECT_CALL(mock_metrics_reporter(),
              Mark(kChangeVisibleModeToLoadingStartMark))
      .Times(1);

  auto state = CreateValidNavigationControlsState();
  state->reload_control_state->is_navigation_loading = true;

  handler().OnNavigationControlsStateChanged(std::move(state));

  page2.FlushForTesting();
  page().FlushForTesting();
}

// Test suite for SplitTabs-related tests.
using BrowserControlsServiceSplitTabsTest = BrowserControlsServiceTest;

// Tests that OnNavigationControlsStateChanged calls the page with the correct
// state.
TEST_F(BrowserControlsServiceSplitTabsTest, TestOnTabSplitStatusChanged) {
  auto state = CreateValidNavigationControlsState();
  state->split_tabs_control_state->is_current_tab_split = true;
  state->split_tabs_control_state->location =
      browser_controls_api::mojom::SplitTabActiveLocation::kStart;

  EXPECT_CALL(
      page(),
      OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
          &browser_controls_api::mojom::NavigationControlsState::
              split_tabs_control_state,
          testing::Pointee(testing::AllOf(
              testing::Field(&browser_controls_api::mojom::
                                 SplitTabsControlState::is_current_tab_split,
                             true),
              testing::Field(
                  &browser_controls_api::mojom::SplitTabsControlState::location,
                  browser_controls_api::mojom::SplitTabActiveLocation::
                      kStart)))))))
      .Times(1);

  handler().OnNavigationControlsStateChanged(std::move(state));

  page().FlushForTesting();
}

// Tests that OnNavigationControlsStateChanged calls the page with the correct
// state.
TEST_F(BrowserControlsServiceSplitTabsTest,
       TestOnSplitTabsButtonPinStateChanged) {
  auto state = CreateValidNavigationControlsState();
  state->split_tabs_control_state->is_pinned = true;

  EXPECT_CALL(
      page(),
      OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
          &browser_controls_api::mojom::NavigationControlsState::
              split_tabs_control_state,
          testing::Pointee(testing::Field(
              &browser_controls_api::mojom::SplitTabsControlState::is_pinned,
              true))))))
      .Times(1);

  handler().OnNavigationControlsStateChanged(std::move(state));

  page().FlushForTesting();
}

// Tests that OnPageInitialized calls the delegate.
TEST_F(BrowserControlsServiceSplitTabsTest, TestOnPageInitializedDelegates) {
  // Delegate OnPageInitialized should be called.
  EXPECT_CALL(delegate(), OnPageInitialized()).Times(1);

  handler().OnPageInitialized();
}
