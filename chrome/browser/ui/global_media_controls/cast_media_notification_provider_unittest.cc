// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_provider.h"

#include "chrome/browser/media/router/test/mock_media_router.h"
#include "components/media_message_center/media_notification_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockMediaNotificationController
    : public media_message_center::MediaNotificationController {
 public:
  MockMediaNotificationController() = default;
  ~MockMediaNotificationController() = default;

  MOCK_METHOD1(ShowNotification, void(const std::string& id));
  MOCK_METHOD1(HideNotification, void(const std::string& id));
  MOCK_METHOD1(RemoveItem, void(const std::string& id));
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override {
    return nullptr;
  }
  MOCK_METHOD1(LogMediaSessionActionButtonPressed, void(const std::string& id));
};

class MockClosure {
 public:
  MOCK_METHOD0(Run, void());
};

}  // namespace

class CastMediaNotificationProviderTest : public testing::Test {
 public:
  void SetUp() override {
    notification_provider_ = std::make_unique<CastMediaNotificationProvider>(
        &router_, &notification_controller_,
        base::BindRepeating(&MockClosure::Run,
                            base::Unretained(&items_changed_callback_)));
  }

  void TearDown() override { notification_provider_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<CastMediaNotificationProvider> notification_provider_;
  MockMediaNotificationController notification_controller_;
  media_router::MockMediaRouter router_;
  MockClosure items_changed_callback_;
};

TEST_F(CastMediaNotificationProviderTest, AddAndRemoveRoute) {
  const std::string route_id = "route-id-1";
  media_router::MediaRoute route(route_id,
                                 media_router::MediaSource("source_id"),
                                 "sink_id", "description", true, true);
  route.set_controller_type(media_router::RouteControllerType::kGeneric);

  EXPECT_CALL(items_changed_callback_, Run());
  notification_provider_->OnRoutesUpdated({route}, {});
  testing::Mock::VerifyAndClearExpectations(&items_changed_callback_);
  EXPECT_TRUE(notification_provider_->HasItems());
  EXPECT_NE(nullptr, notification_provider_->GetNotificationItem(route_id));

  EXPECT_CALL(items_changed_callback_, Run());
  notification_provider_->OnRoutesUpdated({}, {});
  testing::Mock::VerifyAndClearExpectations(&items_changed_callback_);
  EXPECT_FALSE(notification_provider_->HasItems());
}
