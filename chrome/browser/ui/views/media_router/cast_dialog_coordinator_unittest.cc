// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_coordinator.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;

namespace media_router {

class MockCastDialogController : public CastDialogController {
 public:
  MOCK_METHOD(void, AddObserver, (CastDialogController::Observer * observer));
  MOCK_METHOD(void,
              RemoveObserver,
              (CastDialogController::Observer * observer));
  MOCK_METHOD(void,
              StartCasting,
              (const std::string& sink_id, MediaCastMode cast_mode));
  MOCK_METHOD(void, StopCasting, (const std::string& route_id));
  MOCK_METHOD(void, ClearIssue, (const Issue::Id& issue_id));
  MOCK_METHOD(void, FreezeRoute, (const std::string& route_id));
  MOCK_METHOD(void, UnfreezeRoute, (const std::string& route_id));
  MOCK_METHOD(std::unique_ptr<MediaRouteStarter>, TakeMediaRouteStarter, ());
  MOCK_METHOD(void, RegisterDestructor, (base::OnceClosure));
};

class CastDialogCoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override { TestWithBrowserView::SetUp(); }

  NiceMock<MockCastDialogController> controller_;
  CastDialogCoordinator cast_dialog_coordinator_;
};

// Tests show and hide for ShowDialogCenteredForBrowserWindow. Defers
// ShowDialogWithToolbarAction to Media Router tests (already covered) since
// additional Media Router services setup is required.
TEST_F(CastDialogCoordinatorTest, ShowAndHideDialog) {
  EXPECT_CALL(controller_, AddObserver(_));
  cast_dialog_coordinator_.ShowDialogCenteredForBrowserWindow(
      &controller_, browser_view()->browser(), base::Time::Now(),
      MediaRouterDialogActivationLocation::PAGE);
  EXPECT_TRUE(cast_dialog_coordinator_.IsShowing());
  EXPECT_NE(nullptr, cast_dialog_coordinator_.GetCastDialogWidget());
  views::test::WidgetDestroyedWaiter waiter(
      cast_dialog_coordinator_.GetCastDialogWidget());
  EXPECT_CALL(controller_, RemoveObserver(_));
  cast_dialog_coordinator_.Hide();
  waiter.Wait();
  EXPECT_FALSE(cast_dialog_coordinator_.IsShowing());
  EXPECT_EQ(nullptr, cast_dialog_coordinator_.GetCastDialogWidget());
}

}  // namespace media_router
