// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/toolbar_ui_service.h"

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/metrics_reporter/mock_metrics_reporter.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace toolbar_ui_api {

namespace {

using ::testing::_;
using ::testing::Return;

// Measurement marks.
constexpr char kChangeVisibleModeToLoadingStartMark[] =
    "ToolbarUI.ChangeVisibleModeToLoading.Start";
constexpr char kChangeVisibleModeToNotLoadingStartMark[] =
    "ToolbarUI.ChangeVisibleModeToNotLoading.Start";

class MockToolbarUIServiceDelegate
    : public ToolbarUIService::ToolbarUIServiceDelegate {
 public:
  MOCK_METHOD(void,
              HandleContextMenu,
              (mojom::ContextMenuType type,
               gfx::Point location,
               ui::mojom::MenuSourceType source),
              (override));
  MOCK_METHOD(void, OnPageInitialized, (), (override));
};

class Observer : public mojom::ToolbarUIObserver {
 public:
  explicit Observer(ToolbarUIService* service) {
    base::RunLoop run_loop;
    service->Bind(base::BindLambdaForTesting(
        [&](base::expected<mojom::InitialStatePtr, mojo_base::mojom::ErrorPtr>
                result) {
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
  mojo::Receiver<mojom::ToolbarUIObserver> receiver_{this};
};

// This is really an integration test. We provide a faked environment so that we
// can have an easily predictable sealed environment to exercise the
// interactions between our service and dependencies. To validate our
// integration with "real" browser services, we should utilize browser tests.
class ToolbarUIServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto fetcher = std::make_unique<NavigationControlsStateFetcherImpl>(
        base::BindLambdaForTesting(
            [&] { return navigation_controls_state().Clone(); }));
    service_ = std::make_unique<ToolbarUIService>(
        mojo::PendingReceiver<mojom::ToolbarUIService>(), std::move(fetcher),
        &metrics_reporter_, &delegate_);
    observer_ = std::make_unique<Observer>(service_.get());
    observer_->FlushForTesting();
  }

  void TearDown() override { service_.reset(); }

 protected:
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

  ToolbarUIService& service() { return *service_; }
  Observer* observer() { return observer_.get(); }
  MockToolbarUIServiceDelegate& delegate() { return delegate_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ::testing::NiceMock<MockMetricsReporter> metrics_reporter_;
  std::unique_ptr<ToolbarUIService> service_;
  std::unique_ptr<Observer> observer_;
  MockToolbarUIServiceDelegate delegate_;
  mojom::NavigationControlsStatePtr navigation_controls_state_ =
      CreateValidNavigationControlsState();
};

// Tests that calling ShowContextMenu() opens the context menu.
TEST_F(ToolbarUIServiceTest, TestShowContextMenu) {
  EXPECT_CALL(delegate(),
              HandleContextMenu(::testing::_, ::testing::_, ::testing::_));

  service().ShowContextMenu(mojom::ContextMenuType::kReload, gfx::Point(1, 2),
                            ui::mojom::MenuSourceType::kMouse);
}

// Tests that calling OnNavigationControlsStateChanged() calls the page with the
// correct state and records metrics when loading.
TEST_F(ToolbarUIServiceTest, TestOnNavigationStatusChangedLoading) {
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
TEST_F(ToolbarUIServiceTest, TestOnNavigationStatusChangedNotLoading) {
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
TEST_F(ToolbarUIServiceTest, TestOnDevToolsStatusChangedToConnected) {
  ASSERT_FALSE(observer()->state->reload_control_state->is_devtools_connected);

  navigation_controls_state()->reload_control_state->is_devtools_connected =
      true;
  PushNavigationControlsStateUpdate();

  ASSERT_TRUE(observer()->state->reload_control_state->is_devtools_connected);
}

// Tests that multiple observers receive updates.
TEST_F(ToolbarUIServiceTest, MultipleObserversReceiveUpdates) {
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
using ToolbarUIServiceSplitTabsTest = ToolbarUIServiceTest;

// Tests that OnNavigationControlsStateChanged calls the page with the correct
// state.
TEST_F(ToolbarUIServiceSplitTabsTest, TestOnTabSplitStatusChanged) {
  navigation_controls_state()->split_tabs_control_state->is_current_tab_split =
      true;
  navigation_controls_state()->split_tabs_control_state->location =
      toolbar_ui_api::mojom::SplitTabActiveLocation::kStart;
  PushNavigationControlsStateUpdate();

  ASSERT_TRUE(
      observer()->state->split_tabs_control_state->is_current_tab_split);
  ASSERT_EQ(mojom::SplitTabActiveLocation::kStart,
            observer()->state->split_tabs_control_state->location);
}

// Tests that OnNavigationControlsStateChanged calls the page with the correct
// state.
TEST_F(ToolbarUIServiceSplitTabsTest, TestOnSplitTabsButtonPinStateChanged) {
  navigation_controls_state()->split_tabs_control_state->is_pinned = true;
  PushNavigationControlsStateUpdate();

  ASSERT_TRUE(observer()->state->split_tabs_control_state->is_pinned);
}

// Tests that OnPageInitialized calls the delegate.
TEST_F(ToolbarUIServiceSplitTabsTest, TestOnPageInitializedDelegates) {
  // Delegate OnPageInitialized should be called.
  EXPECT_CALL(delegate(), OnPageInitialized()).Times(1);

  service().OnPageInitialized();
}

}  // namespace

}  // namespace toolbar_ui_api
