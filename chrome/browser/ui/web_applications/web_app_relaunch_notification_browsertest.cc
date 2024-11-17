// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_relaunch_notification.h"

#include <set>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/message_center/public/cpp/notification.h"

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::Property;

namespace web_app {

class WebAppRelaunchNotificationBrowserTest
    : public InProcessBrowserTest,
      public NotificationDisplayService::Observer {
 public:
  WebAppRelaunchNotificationBrowserTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {
    GetTaskRunnerForTesting() = task_runner_;
  }

  // NotificationDisplayService::Observer:
  MOCK_METHOD(void,
              OnNotificationDisplayed,
              (const message_center::Notification&,
               const NotificationCommon::Metadata* const),
              (override));
  MOCK_METHOD(void,
              OnNotificationClosed,
              (const std::string& notification_id),
              (override));

  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override {
    notification_observation_.Reset();
  }

  Profile* profile() { return browser()->profile(); }

  auto GetAllNotifications() {
    base::test::TestFuture<std::set<std::string>, bool> get_displayed_future;
    NotificationDisplayServiceFactory::GetForProfile(profile())->GetDisplayed(
        get_displayed_future.GetCallback());

    const auto& notification_ids = get_displayed_future.Get<0>();
    EXPECT_TRUE(get_displayed_future.Wait());
    return notification_ids;
  }

  void ClearAllNotifications() {
    NotificationDisplayService* service =
        NotificationDisplayServiceFactory::GetForProfile(profile());

    for (const std::string& notification_id : GetAllNotifications()) {
      service->Close(NotificationHandler::Type::TRANSIENT, notification_id);
    }
  }

  size_t GetDisplayedNotificationsCount() {
    return GetAllNotifications().size();
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  base::ScopedObservation<NotificationDisplayService,
                          WebAppRelaunchNotificationBrowserTest>
      notification_observation_{this};
};

IN_PROC_BROWSER_TEST_F(WebAppRelaunchNotificationBrowserTest,
                       ShowNotificationOnRelaunch) {
  ClearAllNotifications();
  notification_observation_.Observe(
      NotificationDisplayServiceFactory::GetForProfile(profile()));

  EXPECT_CALL(
      *this,
      OnNotificationDisplayed(
          AllOf(
              Property(&message_center::Notification::id,
                       Eq("web_app_relaunch_notifier:placeholder_app_id")),
              Property(&message_center::Notification::notifier_id,
                       Field(&message_center::NotifierId::id,
                             Eq("web_app_relaunch"))),
              Property(&message_center::Notification::title,
                       Eq(u"Restarting and updating final app title")),
              Property(
                  &message_center::Notification::message,
                  Eq(u"Please wait while this application is being updated"))),
          _))
      .Times(1);

  NotifyAppRelaunchState("placeholder_app_id", "final_app_id",
                         u"final app title", profile()->GetWeakPtr(),
                         AppRelaunchState::kAppClosingForRelaunch);
  EXPECT_EQ(1u, GetDisplayedNotificationsCount());

  NotifyAppRelaunchState("placeholder_app_id", "final_app_id",
                         u"finall app title", profile()->GetWeakPtr(),
                         AppRelaunchState::kAppRelaunched);

  // The notification is still shown due to minimal visibility time after the
  // app was relaunched.
  EXPECT_EQ(1u, GetDisplayedNotificationsCount());

  // After two seconds the notification should be gone.
  task_runner_->FastForwardBy(
      base::Seconds(kSecondsToShowNotificationPostAppRelaunch));
  EXPECT_EQ(0u, GetDisplayedNotificationsCount());
}

IN_PROC_BROWSER_TEST_F(WebAppRelaunchNotificationBrowserTest,
                       TwoAppsInParallelShowNotificationsOnRelaunch) {
  ClearAllNotifications();
  notification_observation_.Observe(
      NotificationDisplayServiceFactory::GetForProfile(profile()));

  EXPECT_CALL(
      *this,
      OnNotificationDisplayed(
          AllOf(
              Property(&message_center::Notification::id,
                       Eq("web_app_relaunch_notifier:placeholder_app_id_1")),
              Property(&message_center::Notification::notifier_id,
                       Field(&message_center::NotifierId::id,
                             Eq("web_app_relaunch"))),
              Property(&message_center::Notification::title,
                       Eq(u"Restarting and updating final app title 1")),
              Property(
                  &message_center::Notification::message,
                  Eq(u"Please wait while this application is being updated"))),
          _))
      .Times(1);

  EXPECT_CALL(
      *this,
      OnNotificationDisplayed(
          AllOf(
              Property(&message_center::Notification::id,
                       Eq("web_app_relaunch_notifier:placeholder_app_id_2")),
              Property(&message_center::Notification::notifier_id,
                       Field(&message_center::NotifierId::id,
                             Eq("web_app_relaunch"))),
              Property(&message_center::Notification::title,
                       Eq(u"Restarting and updating final app title 2")),
              Property(
                  &message_center::Notification::message,
                  Eq(u"Please wait while this application is being updated"))),
          _))
      .Times(1);

  NotifyAppRelaunchState("placeholder_app_id_1", "final_app_id_1",
                         u"final app title 1", profile()->GetWeakPtr(),
                         AppRelaunchState::kAppClosingForRelaunch);
  EXPECT_EQ(1u, GetDisplayedNotificationsCount());

  NotifyAppRelaunchState("placeholder_app_id_2", "final_app_id_2",
                         u"final app title 2", profile()->GetWeakPtr(),
                         AppRelaunchState::kAppClosingForRelaunch);
  EXPECT_EQ(2u, GetDisplayedNotificationsCount());

  NotifyAppRelaunchState("placeholder_app_id_1", "final_app_id_1",
                         u"finall app title 1", profile()->GetWeakPtr(),
                         AppRelaunchState::kAppRelaunched);
  EXPECT_EQ(2u, GetDisplayedNotificationsCount());

  // After two seconds the notification should be gone.
  task_runner_->FastForwardBy(
      base::Seconds(kSecondsToShowNotificationPostAppRelaunch));
  EXPECT_EQ(1u, GetDisplayedNotificationsCount());

  NotifyAppRelaunchState("placeholder_app_id_2", "final_app_id_2",
                         u"finall app title 2", profile()->GetWeakPtr(),
                         AppRelaunchState::kAppRelaunched);
  EXPECT_EQ(1u, GetDisplayedNotificationsCount());

  // After two seconds the notification should be gone.
  task_runner_->FastForwardBy(
      base::Seconds(kSecondsToShowNotificationPostAppRelaunch));
  EXPECT_EQ(0u, GetDisplayedNotificationsCount());
}

}  // namespace web_app
