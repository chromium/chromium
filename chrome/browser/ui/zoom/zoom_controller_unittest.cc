// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zoom/zoom_controller.h"

#include <memory>
#include <optional>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/zoom/test/zoom_test_utils.h"
#include "components/zoom/zoom_observer.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using zoom::ZoomChangedWatcher;
using zoom::ZoomController;
using zoom::ZoomDisableLock;

class ZoomControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    zoom_controller_.reset(new ZoomController(
        web_contents(), web_contents()->GetPrimaryMainFrame()));

    // This call is needed so that the RenderViewHost reports being alive. This
    // is only important for tests that call ZoomController::SetZoomLevel().
    content::RenderViewHostTester::For(rvh())->CreateTestRenderView();
  }

  void TearDown() override {
    zoom_controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<ZoomController> zoom_controller_;
};

TEST_F(ZoomControllerTest, DidNavigateMainFrame) {
  double zoom_level = zoom_controller_->GetZoomLevel();
  ZoomController::ZoomChangedEventData zoom_change_data(
      web_contents(),
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(), zoom_level,
      zoom_level, ZoomController::ZOOM_MODE_DEFAULT, false);
  ZoomChangedWatcher zoom_change_watcher(zoom_controller_.get(),
                                         zoom_change_data);
  content::MockNavigationHandle handle(web_contents());
  handle.set_has_committed(true);
  handle.set_is_in_primary_main_frame(true);
  zoom_controller_->DidFinishNavigation(&handle);
  zoom_change_watcher.Wait();
}

TEST_F(ZoomControllerTest, Observe_ZoomController) {
  double old_zoom_level = zoom_controller_->GetZoomLevel();
  double new_zoom_level = 110.0;

  NavigateAndCommit(GURL("about:blank"));

  // Changing from default to default so the bubble should not be shown.
  ZoomController::ZoomChangedEventData zoom_change_data1(
      web_contents(),
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
      old_zoom_level, old_zoom_level, ZoomController::ZOOM_MODE_ISOLATED,
      false /* can_show_bubble */);

  {
    ZoomChangedWatcher zoom_change_watcher1(zoom_controller_.get(),
                                            zoom_change_data1);
    zoom_controller_->SetZoomMode(ZoomController::ZOOM_MODE_ISOLATED);
    zoom_change_watcher1.Wait();
  }

  ZoomController::ZoomChangedEventData zoom_change_data2(
      web_contents(),
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
      old_zoom_level, new_zoom_level, ZoomController::ZOOM_MODE_ISOLATED,
      true /* can_show_bubble */);

  {
    ZoomChangedWatcher zoom_change_watcher2(zoom_controller_.get(),
                                            zoom_change_data2);
    zoom_controller_->SetZoomLevel(new_zoom_level);
    zoom_change_watcher2.Wait();
  }
}

TEST_F(ZoomControllerTest, ObserveManualZoomCanShowBubble) {
  NavigateAndCommit(GURL("about:blank"));
  double old_zoom_level = zoom_controller_->GetZoomLevel();
  double new_zoom_level1 = old_zoom_level + 0.5;
  double new_zoom_level2 = old_zoom_level + 1.0;

  zoom_controller_->SetZoomMode(zoom::ZoomController::ZOOM_MODE_MANUAL);
  // By default, the zoom controller will send 'true' for can_show_bubble.
  ZoomController::ZoomChangedEventData zoom_change_data1(
      web_contents(),
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
      old_zoom_level, new_zoom_level1, ZoomController::ZOOM_MODE_MANUAL,
      true /* can_show_bubble */);
  {
    ZoomChangedWatcher zoom_change_watcher1(zoom_controller_.get(),
                                            zoom_change_data1);
    zoom_controller_->SetZoomLevel(new_zoom_level1);
    zoom_change_watcher1.Wait();
  }

  // Override default and verify the subsequent event reflects this change.
  zoom_controller_->SetShowsNotificationBubble(false);
  ZoomController::ZoomChangedEventData zoom_change_data2(
      web_contents(),
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
      new_zoom_level1, new_zoom_level2, ZoomController::ZOOM_MODE_MANUAL,
      false /* can_show_bubble */);
  {
    ZoomChangedWatcher zoom_change_watcher2(zoom_controller_.get(),
                                            zoom_change_data2);
    zoom_controller_->SetZoomLevel(new_zoom_level2);
    zoom_change_watcher2.Wait();
  }
}

TEST_F(ZoomControllerTest, ZoomDisableLockBasic) {
  EXPECT_EQ(ZoomController::ZOOM_MODE_DEFAULT, zoom_controller_->zoom_mode());

  // Acquire single lock.
  std::unique_ptr<ZoomDisableLock> lock1 =
      zoom_controller_->CreateZoomDisableLock();
  EXPECT_EQ(ZoomController::ZOOM_MODE_DISABLED, zoom_controller_->zoom_mode());

  // Release lock.
  lock1.reset();
  EXPECT_EQ(ZoomController::ZOOM_MODE_DEFAULT, zoom_controller_->zoom_mode());
}

TEST_F(ZoomControllerTest, ZoomDisableLockMultiple) {
  EXPECT_EQ(ZoomController::ZOOM_MODE_DEFAULT, zoom_controller_->zoom_mode());

  // Acquire Lock 1.
  std::unique_ptr<ZoomDisableLock> lock1 =
      zoom_controller_->CreateZoomDisableLock();
  EXPECT_EQ(ZoomController::ZOOM_MODE_DISABLED, zoom_controller_->zoom_mode());

  // Acquire Lock 2.
  std::unique_ptr<ZoomDisableLock> lock2 =
      zoom_controller_->CreateZoomDisableLock();
  EXPECT_EQ(ZoomController::ZOOM_MODE_DISABLED, zoom_controller_->zoom_mode());

  // Release Lock 1. Mode should stay DISABLED.
  lock1.reset();
  EXPECT_EQ(ZoomController::ZOOM_MODE_DISABLED, zoom_controller_->zoom_mode());

  // Release Lock 2. Mode should restore to DEFAULT.
  lock2.reset();
  EXPECT_EQ(ZoomController::ZOOM_MODE_DEFAULT, zoom_controller_->zoom_mode());
}

TEST_F(ZoomControllerTest, ZoomDisableLockSetModeWhileLocked) {
  EXPECT_EQ(ZoomController::ZOOM_MODE_DEFAULT, zoom_controller_->zoom_mode());

  // Acquire lock.
  std::unique_ptr<ZoomDisableLock> lock1 =
      zoom_controller_->CreateZoomDisableLock();
  EXPECT_EQ(ZoomController::ZOOM_MODE_DISABLED, zoom_controller_->zoom_mode());

  // Attempt to change zoom mode while locked.
  zoom_controller_->SetZoomMode(ZoomController::ZOOM_MODE_ISOLATED);
  // It should remain DISABLED.
  EXPECT_EQ(ZoomController::ZOOM_MODE_DISABLED, zoom_controller_->zoom_mode());

  // Release lock. It should restore to the requested mode (ISOLATED).
  lock1.reset();
  EXPECT_EQ(ZoomController::ZOOM_MODE_ISOLATED, zoom_controller_->zoom_mode());
}

TEST_F(ZoomControllerTest, ZoomDisableLockWeakPtrSafety) {
  // Acquire lock.
  std::unique_ptr<ZoomDisableLock> lock1 =
      zoom_controller_->CreateZoomDisableLock();

  // Destroy the zoom controller first.
  zoom_controller_.reset();

  // Destroy the lock. This should not crash.
  EXPECT_NO_FATAL_FAILURE(lock1.reset());
}

TEST_F(ZoomControllerTest, ReentrantLockAcquisition) {
  EXPECT_EQ(ZoomController::ZOOM_MODE_DEFAULT, zoom_controller_->zoom_mode());

  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetForWebContents(web_contents());
  ASSERT_TRUE(zoom_map);

  std::unique_ptr<zoom::ZoomDisableLock> lock;
  bool callback_ran = false;

  // Subscribe to HostZoomMap changes.
  base::CallbackListSubscription subscription =
      zoom_map->AddZoomLevelChangedCallback(base::BindRepeating(
          [](ZoomController* controller,
             std::unique_ptr<zoom::ZoomDisableLock>* lock_out, bool* ran,
             const content::HostZoomMap::ZoomLevelChange& change) {
            if (!*ran) {
              *ran = true;
              // Trigger re-entrant lock acquisition.
              *lock_out = controller->CreateZoomDisableLock();
            }
          },
          zoom_controller_.get(), &lock, &callback_ran));

  // Change the zoom mode to ISOLATED. This will call SetTemporaryZoomLevel,
  // which will synchronously trigger our callback.
  zoom_controller_->SetZoomMode(ZoomController::ZOOM_MODE_ISOLATED);

  EXPECT_TRUE(callback_ran);

  // Verify that the lock is active and zoom mode is DISABLED.
  // If the re-entrancy bug was present, this would be ZOOM_MODE_ISOLATED.
  EXPECT_EQ(ZoomController::ZOOM_MODE_DISABLED, zoom_controller_->zoom_mode());

  // Release the lock. The zoom mode should restore to ISOLATED (the cached
  // mode).
  lock.reset();
  EXPECT_EQ(ZoomController::ZOOM_MODE_ISOLATED, zoom_controller_->zoom_mode());
}

TEST_F(ZoomControllerTest, ManualToDefaultPreservesZoomLevel) {
  NavigateAndCommit(GURL("http://google.com"));

  // Set a non-default zoom level.
  double non_default_zoom = 2.0;

  // Go to manual mode.
  zoom_controller_->SetZoomMode(ZoomController::ZOOM_MODE_MANUAL);

  // Set zoom level (this updates zoom_level_ in manual mode).
  zoom_controller_->SetZoomLevel(non_default_zoom);
  EXPECT_DOUBLE_EQ(non_default_zoom, zoom_controller_->GetZoomLevel());

  // Transition to default mode.
  zoom_controller_->SetZoomMode(ZoomController::ZOOM_MODE_DEFAULT);

  // The zoom level should be preserved (set to HostZoomMap for the host).
  EXPECT_DOUBLE_EQ(non_default_zoom, zoom_controller_->GetZoomLevel());
}
