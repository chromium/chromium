// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/cast_notification_controller_lacros.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/media_source.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::WithArg;

namespace media_router {

namespace {

constexpr char kRouteId[] = "route_id";

class MockNotificationDisplayService : public NotificationDisplayService {
 public:
  MOCK_METHOD(void,
              Display,
              (NotificationHandler::Type notification_type,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata));
  MOCK_METHOD(void,
              Close,
              (NotificationHandler::Type notification_type,
               const std::string& notification_id));
  MOCK_METHOD(void, GetDisplayed, (DisplayedNotificationsCallback callback));
  MOCK_METHOD(void, AddObserver, (Observer * observer));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer));
};

MediaRoute CreateMediaRoute() {
  return MediaRoute{kRouteId, MediaSource{"source_id"}, "sink_id",
                    "Route description.",
                    /*is_local=*/true};
}

}  // namespace

class CastNotificationControllerLacrosTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ChromeMediaRouterFactory::GetInstance(),
                              base::BindRepeating(&MockMediaRouter::Create));
    profile_ = builder.Build();

    notification_controller_ =
        std::make_unique<CastNotificationControllerLacros>(
            profile_.get(), &notification_service_, &media_router_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  MockNotificationDisplayService notification_service_;
  MockMediaRouter media_router_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CastNotificationControllerLacros> notification_controller_;
};

TEST_F(CastNotificationControllerLacrosTest, DisplayNotification) {
  EXPECT_CALL(notification_service_,
              Display(NotificationHandler::Type::TRANSIENT, _, _))
      .WillOnce(
          WithArg<1>([](const message_center::Notification& notification) {
            EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
                      notification.type());
            EXPECT_EQ("browser.cast.session", notification.id());
            EXPECT_EQ("browser.cast", notification.notifier_id().id);
          }));
  notification_controller_->OnRoutesUpdated({CreateMediaRoute()});
}

TEST_F(CastNotificationControllerLacrosTest, CloseNotification) {
  notification_controller_->OnRoutesUpdated({CreateMediaRoute()});

  // Removing a route should cause the corresponding notification to be closed.
  EXPECT_CALL(notification_service_, Close(NotificationHandler::Type::TRANSIENT,
                                           "browser.cast.session"));
  notification_controller_->OnRoutesUpdated({});
}

TEST_F(CastNotificationControllerLacrosTest, ClickOnStopButton) {
  EXPECT_CALL(media_router_, TerminateRoute(kRouteId));
  EXPECT_CALL(notification_service_,
              Display(NotificationHandler::Type::TRANSIENT, _, _))
      .WillOnce(
          WithArg<1>([](const message_center::Notification& notification) {
            // Clicking on the stop button should call TerminateRoute().
            notification.delegate()->Click(/*button_index=*/0,
                                           /*reply=*/absl::nullopt);
          }));
  notification_controller_->OnRoutesUpdated({CreateMediaRoute()});
}

}  // namespace media_router
