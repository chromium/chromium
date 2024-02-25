// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>
#include <vector>

#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "ash/system/status_area_widget.h"
#include "base/command_line.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/browser/ui/ash/assistant/test_support/test_util.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/public/cpp/switches.h"
#include "content/public/test/browser_test.h"
#include "sandbox/policy/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/controls/button/label_button.h"

namespace ash::assistant {

namespace {

using ::message_center::MessageCenter;
using ::message_center::MessageCenterObserver;

// Please remember to set auth token when *not* running in |kReplay| mode.
constexpr auto kMode = FakeS3Mode::kReplay;

// Update this when you introduce breaking changes to existing tests.
constexpr int kVersion = 1;

// Macros ----------------------------------------------------------------------

#define EXPECT_VISIBLE_NOTIFICATIONS_BY_PREFIXED_ID(prefix_)                  \
  {                                                                           \
    if (!FindVisibleNotificationsByPrefixedId(prefix_).empty()) {             \
      return;                                                                 \
    }                                                                         \
    MockMessageCenterObserver mock_observer;                                  \
    base::ScopedObservation<MessageCenter, MessageCenterObserver>             \
        observation_{&mock_observer};                                         \
    observation_.Observe(MessageCenter::Get());                               \
                                                                              \
    base::RunLoop run_loop;                                                   \
    EXPECT_CALL(mock_observer, OnNotificationAdded)                           \
        .WillOnce(                                                            \
            testing::Invoke([&run_loop](const std::string& notification_id) { \
              if (!FindVisibleNotificationsByPrefixedId(prefix_).empty())     \
                run_loop.QuitClosure().Run();                                 \
            }));                                                              \
    run_loop.Run();                                                           \
  }

// Helpers ---------------------------------------------------------------------

// Returns the status area widget.
StatusAreaWidget* FindStatusAreaWidget() {
  return Shelf::ForWindow(Shell::GetRootWindowForNewWindows())
      ->shelf_widget()
      ->status_area_widget();
}

// Returns the set of Assistant notifications (as indicated by application id).
message_center::NotificationList::Notifications FindAssistantNotifications() {
  return MessageCenter::Get()->FindNotificationsByAppId("assistant");
}

// Returns the visible notification specified by |id|.
message_center::Notification* FindVisibleNotificationById(
    const std::string& id) {
  return MessageCenter::Get()->FindVisibleNotificationById(id);
}

// Returns visible notifications having id starting with |prefix|.
std::vector<message_center::Notification*> FindVisibleNotificationsByPrefixedId(
    const std::string& prefix) {
  std::vector<message_center::Notification*> notifications;
  for (message_center::Notification* notification :
       MessageCenter::Get()->GetVisibleNotifications()) {
    if (base::StartsWith(notification->id(), prefix,
                         base::CompareCase::SENSITIVE)) {
      notifications.push_back(notification);
    }
  }
  return notifications;
}

// Returns the view for the specified |notification|.
message_center::MessageView* FindViewForNotification(
    const message_center::Notification* notification) {
  NotificationListView* notification_list_view =
      FindStatusAreaWidget()
          ->notification_center_tray()
          ->GetNotificationListView();

  // TODO(crbug/1335196): `FindDescendentsOfClass` returning empty list for
  // `NotificationCenterView` even when `MessageView`s exist. Need to
  // investigate and resolve.
  return notification_list_view->GetMessageViewForNotificationId(
      notification->id());
}

// Returns the action buttons for the specified |notification|.
std::vector<views::LabelButton*> FindActionButtonsForNotification(
    const message_center::Notification* notification) {
  auto* notification_view = FindViewForNotification(notification);

  std::vector<views::LabelButton*> action_buttons;
  FindDescendentsOfClass(notification_view, &action_buttons);

  return action_buttons;
}

// Returns the label for the specified |notification| title.
// NOTE: This method assumes that the title string is unique from other strings
// displayed in the notification. This should be safe since we only use this API
// under controlled circumstances.
views::Label* FindTitleLabelForNotification(
    const message_center::Notification* notification) {
  std::vector<views::Label*> labels;
  FindDescendentsOfClass(FindViewForNotification(notification), &labels);
  for (auto* label : labels) {
    if (label->GetText() == notification->title())
      return label;
  }
  return nullptr;
}

// Performs a tap of the specified |view| and waits until the RunLoop idles.
void TapOnAndWait(const views::View* view) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveTouch(view->GetBoundsInScreen().CenterPoint());
  event_generator.PressTouch();
  event_generator.ReleaseTouch();
  base::RunLoop().RunUntilIdle();
}

// Mocks -----------------------------------------------------------------------

class MockMessageCenterObserver
    : public testing::NiceMock<MessageCenterObserver> {
 public:
  // MessageCenterObserver:
  MOCK_METHOD(void,
              OnNotificationAdded,
              (const std::string& notification_id),
              (override));

  MOCK_METHOD(void,
              OnNotificationUpdated,
              (const std::string& notification_id),
              (override));
};

}  // namespace

// AssistantTimersBrowserTest
// --------------------------------------------------

// All tests are disabled because LibAssistant V2 binary does not run on Linux
// bot. To run the tests on gLinux, please add
// `--gtest_also_run_disabled_tests`.
class DISABLED_AssistantTimersBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  DISABLED_AssistantTimersBrowserTest() {
    // Do not log to file in test. Otherwise multiple tests may create/delete
    // the log file at the same time. See http://crbug.com/1307868.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableLibAssistantLogfile);

    // In browser tests, the fake_s3_server uses gRPC framework, which is not
    // allowed in the sandbox by default. Instead of enabling and setting up the
    // gRPC policy, we do not enable sandbox in the tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        sandbox::policy::switches::kNoSandbox);
  }

  DISABLED_AssistantTimersBrowserTest(
      const DISABLED_AssistantTimersBrowserTest&) = delete;
  DISABLED_AssistantTimersBrowserTest& operator=(
      const DISABLED_AssistantTimersBrowserTest&) = delete;

  ~DISABLED_AssistantTimersBrowserTest() override = default;

  void ShowAssistantUi() {
    if (!tester()->IsVisible())
      tester()->PressAssistantKey();
    AppListTestApi().WaitForBubbleWindow(
        /*wait_for_opening_animation=*/true);
  }

  AssistantTestMixin* tester() { return &tester_; }

 private:
  base::test::ScopedRestoreICUDefaultLocale locale_{"en_US"};
  AssistantTestMixin tester_{&mixin_host_, this, embedded_test_server(), kMode,
                             kVersion};
};

// Tests -----------------------------------------------------------------------

// Timer notifications should be dismissed when disabling Assistant in settings.
// Flaky. See https://crbug.com/1196564.
IN_PROC_BROWSER_TEST_F(DISABLED_AssistantTimersBrowserTest,
                       ShouldDismissTimerNotificationsWhenDisablingAssistant) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Confirm no Assistant notifications are currently being shown.
  EXPECT_TRUE(FindAssistantNotifications().empty());

  // Start a timer for one minute.
  tester()->SendTextQuery("Set a timer for 1 minute.");

  // Check for a stable substring of the expected answers.
  tester()->ExpectTextResponse("1 min.");

  // Expect that an Assistant timer notification is now showing.
  EXPECT_VISIBLE_NOTIFICATIONS_BY_PREFIXED_ID("assistant/timer");

  // Disable Assistant.
  tester()->SetAssistantEnabled(false);
  base::RunLoop().RunUntilIdle();

  // Confirm that our Assistant timer notification has been dismissed.
  EXPECT_TRUE(FindAssistantNotifications().empty());
}

// Pressing the "STOP" action button in a timer notification should result in
// the timer being removed.
// Flaky. See https://crbug.com/1196564.
IN_PROC_BROWSER_TEST_F(DISABLED_AssistantTimersBrowserTest,
                       ShouldRemoveTimerWhenStoppingViaNotification) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Confirm no Assistant notifications are currently being shown.
  EXPECT_TRUE(FindAssistantNotifications().empty());

  // Start a timer for five minutes.
  tester()->SendTextQuery("Set a timer for 5 minutes");
  tester()->ExpectTextResponse("5 min.");

  // Confirm that an Assistant timer notification is now showing.
  EXPECT_VISIBLE_NOTIFICATIONS_BY_PREFIXED_ID("assistant/timer");
  auto notifications = FindVisibleNotificationsByPrefixedId("assistant/timer");
  ASSERT_EQ(1u, notifications.size());

  // Find the action buttons for our notification.
  // NOTE: We expect action buttons for "Pause" and "Cancel".
  auto action_buttons = FindActionButtonsForNotification(notifications.at(0));
  EXPECT_EQ(2u, action_buttons.size());

  // Tap the "Cancel" action button in the notification.
  EXPECT_EQ(u"Cancel", action_buttons.at(1)->GetText());
  TapOnAndWait(action_buttons.at(1));

  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Confirm that no timers exist anymore.
  tester()->SendTextQuery("Show my timers");
  tester()->ExpectAnyOfTheseTextResponses({
      "It looks like you don't have any timers set at the moment.",
  });
}

// Verifies that timer notifications are ticked at regular intervals.
IN_PROC_BROWSER_TEST_F(DISABLED_AssistantTimersBrowserTest,
                       ShouldTickNotificationsAtRegularIntervals) {
  // Observe notifications.
  MockMessageCenterObserver mock;
  base::ScopedObservation<MessageCenter, MessageCenterObserver>
      scoped_observation{&mock};
  scoped_observation.Observe(MessageCenter::Get());

  // Show Assistant UI (once ready).
  tester()->StartAssistantAndWaitForReady();
  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Start a timer for five seconds.
  tester()->SendTextQuery("Set a timer for 5 seconds");
  tester()->ExpectTextResponse("5 sec.");

  // We're going to cache the time of the last notification update so that we
  // can verify updates occur within an expected time frame.
  base::Time last_update;

  // Expect and wait for our five second timer notification to be created.
  EXPECT_VISIBLE_NOTIFICATIONS_BY_PREFIXED_ID("assistant/timer");
  last_update = base::Time::Now();

  auto* notification = FindVisibleNotificationById("assistant");
  auto* title_label = FindTitleLabelForNotification(notification);
  auto title = base::UTF16ToUTF8(title_label->GetText());
  EXPECT_EQ("0:05", title);

  // We are going to assert that updates to our notification occur within an
  // expected time frame, allowing a degree of tolerance to reduce flakiness.
  constexpr auto kExpectedMillisBetweenUpdates = 1000;
  constexpr auto kMillisBetweenUpdatesTolerance = 100;

  // We're going to watch notification updates until 5 seconds past fire time.
  std::deque<std::string> expected_titles = {"0:04",  "0:03",  "0:02",  "0:01",
                                             "0:00",  "-0:01", "-0:02", "-0:03",
                                             "-0:04", "-0:05"};
  bool is_first_update = true;

  // Watch |title_label| and await all expected notification updates.
  base::RunLoop notification_update_run_loop;
  auto notification_update_subscription =
      title_label->AddTextChangedCallback(base::BindLambdaForTesting([&]() {
        base::Time now = base::Time::Now();

        // Assert that the update was received within our expected time frame.
        if (is_first_update) {
          is_first_update = false;
          // Our updates are synced to the nearest full second, meaning our
          // first update can come anywhere from 1 ms to 1000 ms from the time
          // our notification was shown.
          EXPECT_LE((now - last_update).InMilliseconds(),
                    1000 + kMillisBetweenUpdatesTolerance);
        } else {
          // Consecutive updates must come regularly.
          EXPECT_NEAR((now - last_update).InMilliseconds(),
                      kExpectedMillisBetweenUpdates,
                      kMillisBetweenUpdatesTolerance);
        }

        // Assert that the notification has the expected title.
        auto title = base::UTF16ToUTF8(title_label->GetText());
        EXPECT_EQ(expected_titles.front(), title);

        // Update time of |last_update|.
        last_update = now;

        // When |expected_titles| is empty, our test is finished.
        expected_titles.pop_front();
        if (expected_titles.empty()) {
          notification_update_run_loop.QuitClosure().Run();
        }
      }));
  notification_update_run_loop.Run();
}

}  // namespace ash::assistant
