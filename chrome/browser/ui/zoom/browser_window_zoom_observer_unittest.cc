// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/zoom/browser_window_zoom_observer.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;

class BrowserWindowZoomObserverTest : public testing::Test {
 protected:
  BrowserWindowZoomObserverTest()
      : tab_strip_model_(&tab_strip_model_delegate_,
                         &profile_,
                         /*group_model_factory=*/nullptr) {
    tab_strip_model_delegate_.SetBrowserWindowInterface(&mock_browser_);
    ON_CALL(mock_browser_, GetProfile()).WillByDefault(Return(&profile_));
    ON_CALL(std::as_const(mock_browser_), GetProfile())
        .WillByDefault(Return(&profile_));
    ON_CALL(mock_browser_, GetTabStripModel())
        .WillByDefault(Return(&tab_strip_model_));
    ON_CALL(std::as_const(mock_browser_), GetTabStripModel())
        .WillByDefault(Return(&tab_strip_model_));

    fullscreen_controller_ =
        std::make_unique<BrowserWindowFullscreenController>(mock_browser_);
    window_feature_controller_ = std::make_unique<WindowFeatureController>(
        fullscreen_controller_.get(), /*app_controller=*/nullptr,
        BrowserWindowInterface::Type::TYPE_NORMAL, /*is_trusted_source=*/true,
        mock_browser_.GetUnownedUserDataHost());
    browser_actions_ = std::make_unique<BrowserActions>(&mock_browser_);
    browser_actions_->InitializeBrowserActions();
    command_controller_ =
        std::make_unique<chrome::BrowserCommandController>(&mock_browser_);
    zoom_observer_ =
        std::make_unique<BrowserWindowZoomObserver>(&mock_browser_);
  }

  ~BrowserWindowZoomObserverTest() override {
    tab_strip_model_.CloseAllTabs();
    zoom_observer_.reset();
    command_controller_.reset();
  }

  // Appends a tab backed by a test WebContents with a ZoomController attached
  // and returns its WebContents.
  content::WebContents* AddTabWithZoomController(bool foreground) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    content::WebContents* raw_contents = contents.get();
    zoom::ZoomController::CreateForWebContents(raw_contents);
    tab_strip_model_.AppendWebContents(std::move(contents), foreground);
    return raw_contents;
  }

  zoom::ZoomController::ZoomChangedEventData MakeZoomEvent(
      content::WebContents* contents,
      bool can_show_bubble) {
    return zoom::ZoomController::ZoomChangedEventData(
        contents, content::FrameTreeNodeId(), /*old_zoom_level=*/0.0,
        /*new_zoom_level=*/1.0, zoom::ZoomController::ZOOM_MODE_DEFAULT,
        can_show_bubble);
  }

  MockBrowserWindowInterface& mock_browser() { return mock_browser_; }
  TabStripModel& tab_strip_model() { return tab_strip_model_; }
  BrowserWindowZoomObserver& zoom_observer() { return *zoom_observer_; }

 private:
  // Tabs inserted into a bare TabStripModel must not spin up the full
  // TabFeatures set, which depends on window features a mock does not provide.
  const tabs::TabModel::PreventFeatureInitializationForTesting
      prevent_tab_features_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  NiceMock<MockBrowserWindowInterface> mock_browser_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  TabStripModel tab_strip_model_;
  std::unique_ptr<BrowserWindowFullscreenController> fullscreen_controller_;
  std::unique_ptr<WindowFeatureController> window_feature_controller_;
  std::unique_ptr<BrowserActions> browser_actions_;
  std::unique_ptr<chrome::BrowserCommandController> command_controller_;
  std::unique_ptr<BrowserWindowZoomObserver> zoom_observer_;
};

TEST_F(BrowserWindowZoomObserverTest, FromReturnsInstance) {
  EXPECT_EQ(&zoom_observer(), BrowserWindowZoomObserver::From(&mock_browser()));

  const BrowserWindowInterface* const_browser = &mock_browser();
  EXPECT_EQ(&zoom_observer(), BrowserWindowZoomObserver::From(const_browser));
}

TEST_F(BrowserWindowZoomObserverTest, ObservesZoomControllerOfInsertedTab) {
  content::WebContents* contents = AddTabWithZoomController(true);
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(contents);
  ASSERT_NE(nullptr, zoom_controller);

  EXPECT_TRUE(
      zoom_observer().IsObservingZoomControllerForTesting(*zoom_controller));
}

TEST_F(BrowserWindowZoomObserverTest, StopsObservingRemovedTab) {
  content::WebContents* contents = AddTabWithZoomController(true);
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(contents);
  ASSERT_NE(nullptr, zoom_controller);
  ASSERT_TRUE(
      zoom_observer().IsObservingZoomControllerForTesting(*zoom_controller));

  // Detach (not delete) so the ZoomController outlives the removal and we can
  // assert the observation was dropped through the TabStripModel path rather
  // than OnZoomControllerDestroyed().
  std::unique_ptr<content::WebContents> detached =
      tab_strip_model().DetachWebContentsAtForInsertion(0);
  EXPECT_FALSE(
      zoom_observer().IsObservingZoomControllerForTesting(*zoom_controller));
}

TEST_F(BrowserWindowZoomObserverTest, ZoomChangeOnActiveTabNotifies) {
  content::WebContents* contents = AddTabWithZoomController(true);
  ASSERT_EQ(contents, tab_strip_model().GetActiveWebContents());

  base::MockCallback<BrowserWindowZoomObserver::ZoomChangedCallback> callback;
  base::CallbackListSubscription subscription =
      zoom_observer().RegisterZoomChangedCallback(callback.Get());

  EXPECT_CALL(callback, Run(/*can_show_bubble=*/true)).Times(1);
  zoom_observer().OnZoomChanged(MakeZoomEvent(contents, true));
}

TEST_F(BrowserWindowZoomObserverTest, ZoomChangeOnBackgroundTabIsIgnored) {
  content::WebContents* active = AddTabWithZoomController(true);
  content::WebContents* background = AddTabWithZoomController(false);
  ASSERT_EQ(active, tab_strip_model().GetActiveWebContents());

  base::MockCallback<BrowserWindowZoomObserver::ZoomChangedCallback> callback;
  base::CallbackListSubscription subscription =
      zoom_observer().RegisterZoomChangedCallback(callback.Get());

  EXPECT_CALL(callback, Run).Times(0);
  zoom_observer().OnZoomChanged(MakeZoomEvent(background, true));
}

TEST_F(BrowserWindowZoomObserverTest, ActiveTabChangeNotifiesWithoutBubble) {
  AddTabWithZoomController(true);
  AddTabWithZoomController(false);

  base::MockCallback<BrowserWindowZoomObserver::ZoomChangedCallback> callback;
  base::CallbackListSubscription subscription =
      zoom_observer().RegisterZoomChangedCallback(callback.Get());

  // Switching tabs refreshes zoom UI but must not pop the zoom bubble.
  EXPECT_CALL(callback, Run(/*can_show_bubble=*/false)).Times(1);
  tab_strip_model().ActivateTabAt(1);
}

TEST_F(BrowserWindowZoomObserverTest, DestroyedSubscriptionStopsCallbacks) {
  content::WebContents* contents = AddTabWithZoomController(true);

  base::MockCallback<BrowserWindowZoomObserver::ZoomChangedCallback> callback;
  {
    base::CallbackListSubscription subscription =
        zoom_observer().RegisterZoomChangedCallback(callback.Get());
    EXPECT_CALL(callback, Run(true)).Times(1);
    zoom_observer().OnZoomChanged(MakeZoomEvent(contents, true));
  }

  EXPECT_CALL(callback, Run).Times(0);
  zoom_observer().OnZoomChanged(MakeZoomEvent(contents, true));
}

}  // namespace
