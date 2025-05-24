// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

class MockTabAlertControllerSubscriber {
 public:
  MockTabAlertControllerSubscriber() = default;

  MOCK_METHOD1(OnPrioritizedAlertStateChanged,
               void(std::optional<TabAlert> new_alert));
};

class TabAlertControllerTest : public testing::Test {
 public:
  TabAlertControllerTest() {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    tab_alert_controller_ =
        std::make_unique<TabAlertController>(web_contents_.get());
  }

  TabAlertController* tab_alert_controller() {
    return tab_alert_controller_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<TabAlertController> tab_alert_controller_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(TabAlertControllerTest, NotifiedOnAlertShouldShowChanged) {
  auto mock_subscriber = std::make_unique<MockTabAlertControllerSubscriber>();
  auto subscription =
      tab_alert_controller()->AddAlertToShowChangedCallback(base::BindRepeating(
          &MockTabAlertControllerSubscriber::OnPrioritizedAlertStateChanged,
          base::Unretained(mock_subscriber.get())));

  // Activating an alert should notify observers since it will be the only
  // tab alert active.
  EXPECT_CALL(*mock_subscriber,
              OnPrioritizedAlertStateChanged(
                  std::make_optional(TabAlert::AUDIO_PLAYING)));
  tab_alert_controller()->OnAudioStateChanged(true);
  ::testing::Mock::VerifyAndClearExpectations(mock_subscriber.get());

  // Simulate a higher priority alert being activated.
  EXPECT_CALL(*mock_subscriber, OnPrioritizedAlertStateChanged(
                                    std::make_optional(TabAlert::PIP_PLAYING)));
  tab_alert_controller()->MediaPictureInPictureChanged(true);
  ::testing::Mock::VerifyAndClearExpectations(mock_subscriber.get());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(), TabAlert::PIP_PLAYING);

  // Removing a lower priority tab alert shouldn't notify observers since the
  // prioritized alert wouldn't change.
  EXPECT_CALL(*mock_subscriber,
              OnPrioritizedAlertStateChanged(std::optional<TabAlert>()))
      .Times(0);
  tab_alert_controller()->OnAudioStateChanged(false);
  ::testing::Mock::VerifyAndClearExpectations(mock_subscriber.get());

  // Remove the last active tab alert.
  EXPECT_CALL(*mock_subscriber, OnPrioritizedAlertStateChanged(testing::_));
  tab_alert_controller()->MediaPictureInPictureChanged(false);
  testing::Mock::VerifyAndClearExpectations(mock_subscriber.get());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(), std::nullopt);
}

TEST_F(TabAlertControllerTest, GetAllAlert) {
  tab_alert_controller()->OnAudioStateChanged(true);
  tab_alert_controller()->OnCapabilityTypesChanged(
      content::WebContentsCapabilityType::kBluetoothConnected, true);
  tab_alert_controller()->MediaPictureInPictureChanged(true);
  tab_alert_controller()->DidUpdateAudioMutingState(true);

  std::optional<TabAlert> prioritized_alert =
      tab_alert_controller()->GetAlertToShow();
  ASSERT_TRUE(prioritized_alert.has_value());
  EXPECT_EQ(prioritized_alert.value(), TabAlert::BLUETOOTH_CONNECTED);
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 4U);

  // Verify that the active alerts list is in sorted order
  std::vector<TabAlert> active_alerts =
      tab_alert_controller()->GetAllActiveAlerts();
  EXPECT_EQ(active_alerts[0], TabAlert::BLUETOOTH_CONNECTED);
  EXPECT_EQ(active_alerts[1], TabAlert::PIP_PLAYING);
  EXPECT_EQ(active_alerts[2], TabAlert::AUDIO_MUTING);
  EXPECT_EQ(active_alerts[3], TabAlert::AUDIO_PLAYING);
}
}  // namespace tabs
