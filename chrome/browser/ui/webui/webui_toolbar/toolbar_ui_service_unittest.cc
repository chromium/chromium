// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/toolbar_ui_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
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
#include "ui/gfx/geometry/rect_f.h"

namespace toolbar_ui_api {

namespace {

using ::testing::_;
using ::testing::Return;

// Measurement marks.
constexpr char kChangeVisibleModeToLoadingStartMark[] =
    "ToolbarUI.ChangeVisibleModeToLoading.Start";
constexpr char kChangeVisibleModeToNotLoadingStartMark[] =
    "ToolbarUI.ChangeVisibleModeToNotLoading.Start";

class Observer : public mojom::ToolbarUIObserver {
 public:
  explicit Observer(ToolbarUIService* service) {
    base::RunLoop run_loop;
    service->Bind(base::BindLambdaForTesting(
        [&](base::expected<mojom::InitialStatePtr, mojo_base::mojom::ErrorPtr>
                result) {
          ASSERT_TRUE(result.has_value());
          icons = std::move(result.value()->icons);
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
      std::vector<mojom::IconUpdatePtr> icons_update,
      mojom::NavigationControlsStatePtr changed) override {
    icons = std::move(icons_update);
    state = std::move(changed);
  }

  void OnFocusRequested(
      toolbar_ui_api::mojom::FocusRequestTarget target) override {}

  void FlushForTesting() { receiver_.FlushForTesting(); }

  // Easily accessible for testing. Start with nullopt to easily differentiate
  // between uninitialized and unset.
  std::optional<std::vector<mojom::IconUpdatePtr>> icons;
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
  explicit ToolbarUIServiceTest(bool auto_add_observer = true)
      : auto_add_observer_(auto_add_observer) {}

  void SetUp() override {
    auto fetcher = std::make_unique<NavigationControlsStateFetcherImpl>(
        base::BindLambdaForTesting(
            [&] { return navigation_controls_state().Clone(); }));
    auto icon_table_fetcher = std::make_unique<FakeIconTableFetcher>();
    icon_table_fetcher_ = icon_table_fetcher.get();
    service_ = std::make_unique<ToolbarUIService>(
        mojo::PendingReceiver<mojom::ToolbarUIService>(), std::move(fetcher),
        std::move(icon_table_fetcher), &metrics_reporter_, &delegate_);
    if (auto_add_observer_) {
      AddInitialObserver();
    }
  }

  void AddInitialObserver() {
    DCHECK(!observer_);
    observer_ = std::make_unique<Observer>(service_.get());
    observer_->FlushForTesting();
  }

  void TearDown() override {
    icon_table_fetcher_ = nullptr;
    service_.reset();
  }

 protected:
  ::testing::NiceMock<MockMetricsReporter>& mock_metrics_reporter() {
    return metrics_reporter_;
  }

  mojom::NavigationControlsStatePtr& navigation_controls_state() {
    return navigation_controls_state_;
  }

  // Updates the service with the current navigation control state.
  void PushNavigationControlsStateUpdate() {
    service_->OnNavigationControlsStateChanged(*navigation_controls_state_);
    if (observer()) {
      observer()->FlushForTesting();
    }
  }

  ToolbarUIService& service() { return *service_; }
  Observer* observer() { return observer_.get(); }
  MockToolbarUIServiceDelegate& delegate() { return delegate_; }
  FakeIconTableFetcher* fake_icon_table() { return icon_table_fetcher_.get(); }

 private:
  bool auto_add_observer_;
  content::BrowserTaskEnvironment task_environment_;
  ::testing::NiceMock<MockMetricsReporter> metrics_reporter_;
  std::unique_ptr<ToolbarUIService> service_;
  raw_ptr<FakeIconTableFetcher> icon_table_fetcher_;  // owned by service_;
  std::unique_ptr<Observer> observer_;
  MockToolbarUIServiceDelegate delegate_;
  mojom::NavigationControlsStatePtr navigation_controls_state_ =
      CreateValidNavigationControlsState();
};

class ToolbarUIServiceNoInitialObserverTest : public ToolbarUIServiceTest {
 public:
  ToolbarUIServiceNoInitialObserverTest()
      : ToolbarUIServiceTest(/*auto_add_observer=*/false) {}
};

// Tests that calling ShowContextMenu() opens the context menu.
TEST_F(ToolbarUIServiceTest, TestShowContextMenu) {
  EXPECT_CALL(delegate(),
              HandleContextMenu(::testing::_, ::testing::_, ::testing::_));

  service().ShowContextMenu(mojom::ContextMenuType::kReload,
                            gfx::RectF(1, 2, 3, 4),
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
TEST_F(ToolbarUIServiceTest, TestOnCanShowMenuChangedToTrue) {
  ASSERT_FALSE(observer()->state->reload_control_state->can_show_menu);

  navigation_controls_state()->reload_control_state->can_show_menu = true;
  PushNavigationControlsStateUpdate();

  ASSERT_TRUE(observer()->state->reload_control_state->can_show_menu);
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

// Tests that calling InvokePinnedToolbarAction() calls the delegate.
TEST_F(ToolbarUIServiceTest, TestInvokePinnedToolbarAction) {
  EXPECT_CALL(delegate(),
              InvokePinnedToolbarAction(mojom::PinnedToolbarAction::kPrint));

  service().InvokePinnedToolbarAction(mojom::PinnedToolbarAction::kPrint);
}

TEST_F(ToolbarUIServiceTest, IconUpdates) {
  auto icon1 = mojom::IconUpdate::New(
      1u, "a.png", mojom::IconType::kFullColorUrl, /*color=*/std::nullopt);
  auto icon2 = mojom::IconUpdate::New(
      2u, "icon-set:puppy", mojom::IconType::kIconSet, /*color=*/std::nullopt);
  auto icon3 = mojom::IconUpdate::New(
      3u, "icon-set:kitten", mojom::IconType::kIconSet, /*color=*/std::nullopt);

  fake_icon_table()->AddUpdate(icon1.Clone());
  PushNavigationControlsStateUpdate();
  ASSERT_TRUE(observer()->icons);
  EXPECT_THAT(*observer()->icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon1))));

  // Add a second observer. It should get the first icon as its initial state.
  Observer observer2(&service());
  ASSERT_TRUE(observer2.icons);
  EXPECT_THAT(*observer2.icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon1))));

  // Now add a couple more icons. Both observers should get them.
  // (But not 1 again, since it's not new).
  fake_icon_table()->AddUpdate(icon2.Clone());
  fake_icon_table()->AddUpdate(icon3.Clone());
  PushNavigationControlsStateUpdate();

  ASSERT_TRUE(observer()->icons);
  EXPECT_THAT(*observer()->icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon2)),
                                   MatchesIconUpdate(std::ref(icon3))));

  ASSERT_TRUE(observer2.icons);
  EXPECT_THAT(*observer2.icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon2)),
                                   MatchesIconUpdate(std::ref(icon3))));
}

// Test with a second observer joining in between updates.
TEST_F(ToolbarUIServiceTest, IconUpdates2) {
  auto icon1 = mojom::IconUpdate::New(
      1u, "a.png", mojom::IconType::kFullColorUrl, /*color=*/std::nullopt);
  auto icon2 = mojom::IconUpdate::New(
      2u, "icon-set:puppy", mojom::IconType::kIconSet, /*color=*/std::nullopt);
  auto icon3 = mojom::IconUpdate::New(
      3u, "icon-set:kitten", mojom::IconType::kIconSet, /*color=*/std::nullopt);

  fake_icon_table()->AddUpdate(icon1.Clone());
  PushNavigationControlsStateUpdate();
  ASSERT_TRUE(observer()->icons);
  EXPECT_THAT(*observer()->icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon1))));

  fake_icon_table()->AddUpdate(icon2.Clone());

  // Add a second observer. It should get the current state of the table as
  // its initial state, so two icons.
  Observer observer2(&service());
  ASSERT_TRUE(observer2.icons);
  EXPECT_THAT(*observer2.icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon1)),
                                   MatchesIconUpdate(std::ref(icon2))));

  // Now add the 3rd icon, and push an update.
  fake_icon_table()->AddUpdate(icon3.Clone());
  PushNavigationControlsStateUpdate();

  // The update message includes both icons, though the second observer already
  // knows about `icon2`. This is OK since these are idempotent.
  ASSERT_TRUE(observer()->icons);
  EXPECT_THAT(*observer()->icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon2)),
                                   MatchesIconUpdate(std::ref(icon3))));

  ASSERT_TRUE(observer2.icons);
  EXPECT_THAT(*observer2.icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon2)),
                                   MatchesIconUpdate(std::ref(icon3))));
}

// Test for icon updates before connect.
TEST_F(ToolbarUIServiceNoInitialObserverTest, IconUpdatesBeforeConnect) {
  auto icon1 = mojom::IconUpdate::New(
      1u, "a.png", mojom::IconType::kFullColorUrl, /*color=*/std::nullopt);

  fake_icon_table()->AddUpdate(icon1.Clone());

  // No observer here yet, so this doesn't notify anyone.
  PushNavigationControlsStateUpdate();

  AddInitialObserver();
  // Now observer gets the icon as initial state.
  ASSERT_TRUE(observer()->icons);
  EXPECT_THAT(*observer()->icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon1))));

  // Pushing doesn't re-send it.
  PushNavigationControlsStateUpdate();
  ASSERT_TRUE(observer()->icons);
  EXPECT_THAT(*observer()->icons, testing::ElementsAre());
}

// Test for icon updates before connect. Variant where no push attempt
// was made.
TEST_F(ToolbarUIServiceNoInitialObserverTest, IconUpdatesBeforeConnect2) {
  auto icon1 = mojom::IconUpdate::New(
      1u, "a.png", mojom::IconType::kFullColorUrl, /*color=*/std::nullopt);

  fake_icon_table()->AddUpdate(icon1.Clone());

  AddInitialObserver();
  // The observer gets the icon as initial state.
  ASSERT_TRUE(observer()->icons);
  EXPECT_THAT(*observer()->icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon1))));

  // It gets redundantly re-sent with the first update, but that's not
  // a big deal since it's idempotent.
  PushNavigationControlsStateUpdate();
  ASSERT_TRUE(observer()->icons);
  EXPECT_THAT(*observer()->icons,
              testing::ElementsAre(MatchesIconUpdate(std::ref(icon1))));
}

// Tests that calling ShowAvatarMenu() calls the delegate.
TEST_F(ToolbarUIServiceTest, TestShowAvatarMenu) {
  EXPECT_CALL(delegate(), ShowAvatarMenu());

  service().ShowAvatarMenu(base::NullCallback());
}

}  // namespace

}  // namespace toolbar_ui_api
