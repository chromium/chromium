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
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/testing/toy_browser.h"
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

namespace browser_controls_api {

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::InvokeArgument;
using ::testing::Return;
using testing::ToyBrowser;

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

class MockWebWebUIToolbarDelegate : public BrowserControlsService::Delegate {
 public:
  MockWebWebUIToolbarDelegate() = default;

  MOCK_METHOD(void,
              HandleContextMenu,
              (mojom::ContextMenuType, gfx::Point, ui::mojom::MenuSourceType),
              (override));
  MOCK_METHOD(void, OnPageInitialized, (), (override));
  MOCK_METHOD(void, PermitLaunchUrl, (), (override));
};

class Observer : public browser_controls_api::mojom::BrowserControlsObserver {
 public:
  explicit Observer(BrowserControlsService* service) {
    base::RunLoop run_loop;
    service->Bind(base::BindLambdaForTesting(
        [&](base::expected<browser_controls_api::mojom::InitialStatePtr,
                           mojo_base::mojom::ErrorPtr> result) {
          ASSERT_TRUE(result.has_value());
          state = std::move(result.value()->state);
          receiver_.Bind(std::move(result.value()->update_stream));
          run_loop.Quit();
        }));
    run_loop.Run();

    FlushForTesting();
  }
  ~Observer() override = default;

  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;

  void OnNavigationControlsStateChanged(
      mojom::NavigationControlsStatePtr changed) override {
    state = std::move(changed);
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  // Easily accessible for testing. Start with nullopt to easily differentiate
  // between uninitialized and unset.
  mojom::NavigationControlsStatePtr state;

 private:
  mojo::Receiver<browser_controls_api::mojom::BrowserControlsObserver>
      receiver_{this};
};

// This is really an integration test. We provide a faked environment so that we
// can have an easily predictable sealed environment to exercise the
// interactions between our service and dependencies. To validate our
// integration with "real" browser services, we should utilize browser tests.
class BrowserControlsServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto fetcher = std::make_unique<NavigationControlsStateFetcherImpl>(
        base::BindLambdaForTesting(
            [&] { return navigation_controls_state().Clone(); }));
    service_ = std::make_unique<BrowserControlsService>(
        mojo::PendingReceiver<mojom::BrowserControlsService>(),
        toy_browser_.GetAdapter(), std::move(fetcher), &metrics_reporter_,
        &delegate_);
    observer_ = std::make_unique<Observer>(service_.get());
    observer_->FlushForTesting();
  }

  void TearDown() override { service_.reset(); }

 protected:
  void ExpectMeasureAndClearMark(const std::string& start_mark,
                                 base::TimeDelta duration) {
    EXPECT_CALL(mock_metrics_reporter(),
                Measure(Eq(start_mark), ::testing::A<base::TimeTicks>(), _))
        .WillOnce(base::test::RunOnceCallback<2>(duration));
    EXPECT_CALL(mock_metrics_reporter(), ClearMark(start_mark));
  }

  void ExpectNoMeasureCallback(const std::string& start_mark) {
    EXPECT_CALL(mock_metrics_reporter(),
                Measure(Eq(start_mark), ::testing::A<base::TimeTicks>(), _))
        .Times(1);
    // OnMeasureResultAndClearMark() calls ClearMark(). Expecting ClearMark() to
    // not be called ensures that the callback is not triggered.
    EXPECT_CALL(mock_metrics_reporter(), ClearMark(Eq(start_mark))).Times(0);
  }

  ::testing::NiceMock<MockMetricsReporter>& mock_metrics_reporter() {
    return metrics_reporter_;
  }

  mojom::NavigationControlsStatePtr& navigation_controls_state() {
    return navigation_controls_state_;
  }
  // Updates the service with the current navigation control state.
  void PushNavigationControlsStateUpdate() {
    service_->OnNavigationControlsStateChanged(
        navigation_controls_state_.Clone());
    observer()->FlushForTesting();
  }

  ToyBrowser& toy_browser() { return toy_browser_; }
  BrowserControlsService& service() { return *service_; }
  Observer* observer() { return observer_.get(); }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  MockWebWebUIToolbarDelegate& delegate() { return delegate_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  testing::ToyBrowser toy_browser_;
  ::testing::NiceMock<MockMetricsReporter> metrics_reporter_;
  std::unique_ptr<BrowserControlsService> service_;
  std::unique_ptr<Observer> observer_;
  base::HistogramTester histogram_tester_;
  MockWebWebUIToolbarDelegate delegate_;
  mojom::NavigationControlsStatePtr navigation_controls_state_ =
      CreateValidNavigationControlsState();
};

// Test suite for Reload-related tests.
using BrowserControlsServiceReloadTest = BrowserControlsServiceTest;

// Tests that calling Reload(false, {}) executes the IDC_RELOAD command and
// records metrics.
TEST_F(BrowserControlsServiceReloadTest, ReloadByMouseRelease) {
  const base::TimeDelta duration = base::Milliseconds(10);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  service().ReloadFromClick(/*bypass_cache=*/false, /*click_flags=*/{});

  EXPECT_EQ(IDC_RELOAD, toy_browser().received_commands().back().command_id);

  histogram_tester().ExpectUniqueTimeSample(kInputToReloadMouseReleaseHistogram,
                                            duration, 1);
}

// Tests that calling Reload(false, {}) doesn't record metrics if the start mark
// is not present.
TEST_F(BrowserControlsServiceReloadTest, ReloadByMouseReleaseNoStartMark) {
  ExpectNoMeasureCallback(kInputMouseReleaseStartMark);

  service().ReloadFromClick(/*bypass_cache=*/false, /*click_flags=*/{});

  EXPECT_EQ(IDC_RELOAD, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            toy_browser().received_commands().back().disposition);

  histogram_tester().ExpectTotalCount(kInputToReloadMouseReleaseHistogram, 0);
}

// Tests that calling Reload(false, {middle_button}) executes the
// IDC_RELOAD with new background tab.
TEST_F(BrowserControlsServiceReloadTest, ReloadWithMiddleMouseButton) {
  const base::TimeDelta duration = base::Milliseconds(10);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  service().ReloadFromClick(
      /*bypass_cache=*/false,
      /*click_flags=*/{mojom::ClickDispositionFlag::kMiddleMouseButton});

  EXPECT_EQ(IDC_RELOAD, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            toy_browser().received_commands().back().disposition);

  histogram_tester().ExpectUniqueTimeSample(kInputToReloadMouseReleaseHistogram,
                                            duration, 1);
}

TEST_F(BrowserControlsServiceReloadTest, ReloadBypassingCache) {
  service().ReloadFromClick(/*bypass_cache=*/true, /*click_flags=*/{});

  EXPECT_EQ(1ul, toy_browser().received_commands().size());
  EXPECT_EQ(IDC_RELOAD_BYPASSING_CACHE,
            toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            toy_browser().received_commands().back().disposition);
}

// Test suite for StopLoad-related tests.
using BrowserControlsServiceStopLoadTest = BrowserControlsServiceTest;

// Tests that calling StopLoad() executes the IDC_STOP command and records
// metrics.
TEST_F(BrowserControlsServiceStopLoadTest, StopLoad) {
  const base::TimeDelta duration = base::Milliseconds(20);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  service().StopLoad();

  EXPECT_EQ(IDC_STOP, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            toy_browser().received_commands().back().disposition);

  histogram_tester().ExpectUniqueTimeSample(kInputToStopMouseReleaseHistogram,
                                            duration, 1);
}

// Tests that calling ShowContextMenu() opens the context menu.
TEST_F(BrowserControlsServiceTest, TestShowContextMenu) {
  EXPECT_CALL(delegate(),
              HandleContextMenu(::testing::_, ::testing::_, ::testing::_));

  service().ShowContextMenu(mojom::ContextMenuType::kReload, gfx::Point(1, 2),
                            ui::mojom::MenuSourceType::kMouse);
}

// Tests that calling OnNavigationControlsStateChanged() calls the page with the
// correct state and records metrics when loading.
TEST_F(BrowserControlsServiceTest, TestOnNavigationStatusChangedLoading) {
  EXPECT_CALL(mock_metrics_reporter(),
              Mark(kChangeVisibleModeToLoadingStartMark))
      .Times(1);
  ASSERT_FALSE(observer()->state->reload_control_state->is_navigation_loading);

  navigation_controls_state()->reload_control_state->is_navigation_loading =
      true;
  PushNavigationControlsStateUpdate();

  ASSERT_TRUE(observer()->state->reload_control_state->is_navigation_loading);
}

// Tests that calling OnNavigationControlsStateChanged() calls the page with the
// correct state and records metrics when not loading.
TEST_F(BrowserControlsServiceTest, TestOnNavigationStatusChangedNotLoading) {
  EXPECT_CALL(mock_metrics_reporter(),
              Mark(kChangeVisibleModeToNotLoadingStartMark))
      .Times(1);

  navigation_controls_state()->reload_control_state->is_navigation_loading =
      false;
  PushNavigationControlsStateUpdate();

  ASSERT_FALSE(observer()->state->reload_control_state->is_navigation_loading);
}

// Tests that calling OnNavigationControlsStateChanged() calls the page with the
// correct state.
TEST_F(BrowserControlsServiceTest, TestOnDevToolsStatusChangedToConnected) {
  ASSERT_FALSE(observer()->state->reload_control_state->is_devtools_connected);

  navigation_controls_state()->reload_control_state->is_devtools_connected =
      true;
  PushNavigationControlsStateUpdate();

  ASSERT_TRUE(observer()->state->reload_control_state->is_devtools_connected);
}

// Tests that multiple observers receive updates.
TEST_F(BrowserControlsServiceTest, MultipleObserversReceiveUpdates) {
  Observer observer2(&service());

  EXPECT_CALL(mock_metrics_reporter(),
              Mark(kChangeVisibleModeToLoadingStartMark))
      .Times(1);

  ASSERT_FALSE(observer()->state->reload_control_state->is_navigation_loading);
  ASSERT_FALSE(observer2.state->reload_control_state->is_navigation_loading);

  navigation_controls_state()->reload_control_state->is_navigation_loading =
      true;
  PushNavigationControlsStateUpdate();
  observer2.FlushForTesting();

  ASSERT_TRUE(observer()->state->reload_control_state->is_navigation_loading);
  ASSERT_TRUE(observer2.state->reload_control_state->is_navigation_loading);
}

// Test suite for SplitTabs-related tests.
using BrowserControlsServiceSplitTabsTest = BrowserControlsServiceTest;

// Tests that OnNavigationControlsStateChanged calls the page with the correct
// state.
TEST_F(BrowserControlsServiceSplitTabsTest, TestOnTabSplitStatusChanged) {
  navigation_controls_state()->split_tabs_control_state->is_current_tab_split =
      true;
  navigation_controls_state()->split_tabs_control_state->location =
      browser_controls_api::mojom::SplitTabActiveLocation::kStart;
  PushNavigationControlsStateUpdate();

  ASSERT_TRUE(
      observer()->state->split_tabs_control_state->is_current_tab_split);
  ASSERT_EQ(mojom::SplitTabActiveLocation::kStart,
            observer()->state->split_tabs_control_state->location);
}

// Tests that OnNavigationControlsStateChanged calls the page with the correct
// state.
TEST_F(BrowserControlsServiceSplitTabsTest,
       TestOnSplitTabsButtonPinStateChanged) {
  navigation_controls_state()->split_tabs_control_state->is_pinned = true;
  PushNavigationControlsStateUpdate();

  ASSERT_TRUE(observer()->state->split_tabs_control_state->is_pinned);
}

// Tests that OnPageInitialized calls the delegate.
TEST_F(BrowserControlsServiceSplitTabsTest, TestOnPageInitializedDelegates) {
  // Delegate OnPageInitialized should be called.
  EXPECT_CALL(delegate(), OnPageInitialized()).Times(1);

  service().OnPageInitialized();
}

}  // namespace
}  // namespace browser_controls_api
