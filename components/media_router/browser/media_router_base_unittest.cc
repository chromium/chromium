// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_base.h"

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/browser/test/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::PresentationConnectionState;
using testing::_;
using testing::SaveArg;

namespace media_router {

// MockMediaRouterBase inherits from MediaRouter but overrides some of its
// methods with mock methods, so we must override them again.
class MockMediaRouterBase : public MockMediaRouter {
 public:
  MockMediaRouterBase() = default;
  ~MockMediaRouterBase() override = default;

  base::CallbackListSubscription AddPresentationConnectionStateChangedCallback(
      const MediaRoute::Id& route_id,
      const content::PresentationConnectionStateChangedCallback& callback)
      override {
    return MediaRouterBase::AddPresentationConnectionStateChangedCallback(
        route_id, callback);
  }
};

class MediaRouterBaseTest : public testing::Test {
 public:
  void SetUp() override { router_.Initialize(); }

  void TearDown() override { router_.Shutdown(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  MockMediaRouterBase router_;
};

TEST_F(MediaRouterBaseTest, CreatePresentationIds) {
  std::string id1 = MediaRouterBase::CreatePresentationId();
  std::string id2 = MediaRouterBase::CreatePresentationId();
  EXPECT_NE(id1, "");
  EXPECT_NE(id2, "");
  EXPECT_NE(id1, id2);
}

TEST_F(MediaRouterBaseTest, NotifyCallbacks) {
  MediaRoute::Id route_id1("id1");
  MediaRoute::Id route_id2("id2");
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback1;
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback2;
  base::CallbackListSubscription subscription1 =
      router_.AddPresentationConnectionStateChangedCallback(route_id1,
                                                            callback1.Get());
  base::CallbackListSubscription subscription2 =
      router_.AddPresentationConnectionStateChangedCallback(route_id2,
                                                            callback2.Get());

  content::PresentationConnectionStateChangeInfo change_info_connected(
      PresentationConnectionState::CONNECTED);
  content::PresentationConnectionStateChangeInfo change_info_terminated(
      PresentationConnectionState::TERMINATED);
  content::PresentationConnectionStateChangeInfo change_info_closed(
      PresentationConnectionState::CLOSED);
  change_info_closed.close_reason =
      blink::mojom::PresentationConnectionCloseReason::WENT_AWAY;
  change_info_closed.message = "Test message";

  EXPECT_CALL(callback1, Run(StateChangeInfoEquals(change_info_connected)));
  router_.NotifyPresentationConnectionStateChange(
      route_id1, PresentationConnectionState::CONNECTED);

  EXPECT_CALL(callback2, Run(StateChangeInfoEquals(change_info_connected)));
  router_.NotifyPresentationConnectionStateChange(
      route_id2, PresentationConnectionState::CONNECTED);

  EXPECT_CALL(callback1, Run(StateChangeInfoEquals(change_info_closed)));
  router_.NotifyPresentationConnectionClose(
      route_id1, change_info_closed.close_reason, change_info_closed.message);

  // After removing a subscription, the corresponding callback should no longer
  // be called.
  subscription1 = {};
  router_.NotifyPresentationConnectionStateChange(
      route_id1, PresentationConnectionState::TERMINATED);

  EXPECT_CALL(callback2, Run(StateChangeInfoEquals(change_info_terminated)));
  router_.NotifyPresentationConnectionStateChange(
      route_id2, PresentationConnectionState::TERMINATED);

  subscription2 = {};
  router_.NotifyPresentationConnectionStateChange(
      route_id2, PresentationConnectionState::TERMINATED);
}

}  // namespace media_router
