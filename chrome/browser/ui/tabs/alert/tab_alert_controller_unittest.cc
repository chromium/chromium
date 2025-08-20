// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace tabs {

class FakeBrowserWindowInterface : public MockBrowserWindowInterface {
 public:
  ~FakeBrowserWindowInterface() override = default;
  explicit FakeBrowserWindowInterface(Profile* profile) : profile_(profile) {}
  Profile* GetProfile() override { return profile_; }

 private:
  raw_ptr<Profile> profile_ = nullptr;
};

class MockTabAlertControllerSubscriber {
 public:
  MockTabAlertControllerSubscriber() = default;

  MOCK_METHOD1(OnPrioritizedAlertStateChanged,
               void(std::optional<TabAlert> new_alert));
};

class TabAlertControllerTest : public testing::Test {
 public:
  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
    profile_ = testing_profile_manager_->CreateTestingProfile("profile");
    browser_window_interface_ =
        std::make_unique<FakeBrowserWindowInterface>(profile_);
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_delegate_->SetBrowserWindowInterface(
        browser_window_interface_.get());
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), profile_);
    EXPECT_CALL(*browser_window_interface_, GetTabStripModel())
        .WillRepeatedly(testing::Return(tab_strip_model_.get()));
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
    tab_model_ = std::make_unique<TabModel>(std::move(web_contents),
                                            tab_strip_model_.get());
  }

  void TearDown() override {
    // Explicitly reset the pointers to prevent them from causing the
    // BrowserTaskEnvironment to time out on destruction.
    tab_model_.reset();
    tab_strip_model_.reset();
    tab_strip_model_delegate_.reset();
    browser_window_interface_.reset();
    profile_ = nullptr;
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();
    testing_profile_manager_.reset();
  }

  TabAlertController* tab_alert_controller() {
    return tabs::TabAlertController::From(tab_model_.get());
  }

  TabInterface* tab_interface() { return tab_model_.get(); }

  void SimulateAudioState(bool is_playing_audio) {
    content::WebContentsTester::For(tab_model_->GetContents())
        ->SetIsCurrentlyAudible(is_playing_audio);
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler test_enabler_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<FakeBrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<TabModel> tab_model_;
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
  SimulateAudioState(true);
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
  SimulateAudioState(false);
  task_environment()->FastForwardBy(base::Seconds(2));
  ::testing::Mock::VerifyAndClearExpectations(mock_subscriber.get());

  // Remove the last active tab alert.
  EXPECT_CALL(*mock_subscriber, OnPrioritizedAlertStateChanged(testing::_));
  tab_alert_controller()->MediaPictureInPictureChanged(false);
  testing::Mock::VerifyAndClearExpectations(mock_subscriber.get());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(), std::nullopt);
}

TEST_F(TabAlertControllerTest, GetAllAlert) {
  SimulateAudioState(true);
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

TEST_F(TabAlertControllerTest, AlertIsActive) {
  SimulateAudioState(true);
  tab_alert_controller()->OnCapabilityTypesChanged(
      content::WebContentsCapabilityType::kBluetoothConnected, true);
  tab_alert_controller()->MediaPictureInPictureChanged(true);

  EXPECT_TRUE(tab_alert_controller()->IsAlertActive(TabAlert::AUDIO_PLAYING));
  EXPECT_TRUE(
      tab_alert_controller()->IsAlertActive(TabAlert::BLUETOOTH_CONNECTED));
  EXPECT_TRUE(tab_alert_controller()->IsAlertActive(TabAlert::PIP_PLAYING));

  // When the non-prioritized alert is no longer active, the alert controller
  // should be updated to reflect that.
  tab_alert_controller()->MediaPictureInPictureChanged(false);
  EXPECT_FALSE(tab_alert_controller()->IsAlertActive(TabAlert::PIP_PLAYING));
}

TEST_F(TabAlertControllerTest, VrStateUpdatesAlertController) {
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
  vr::VrTabHelper* const vr_tab_helper =
      vr::VrTabHelper::FromWebContents(tab_interface()->GetContents());
  vr_tab_helper->SetIsContentDisplayedInHeadset(true);
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::VR_PRESENTING_IN_HEADSET);
  vr_tab_helper->SetIsContentDisplayedInHeadset(false);
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}

TEST_F(TabAlertControllerTest, AudioStateUpdatesAlertController) {
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
  SimulateAudioState(true);
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::AUDIO_PLAYING);

  // The audio playing alert should still be active even though the audio has
  // stopped to prevent the audio state from toggling too frequently on pause.
  SimulateAudioState(false);
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::AUDIO_PLAYING);

  // The tab alert should go away after 2 seconds of consistently not playing
  // audio.
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}

TEST_F(TabAlertControllerTest, MutedStateReliesOnRecentlyAudible) {
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
  tab_interface()->GetContents()->SetAudioMuted(true);
  // Even though the tab is muted, since it wasn't recently audible, the muted
  // tab alert shouldn't be active.
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());

  // Simulating the tab to be audible should trigger the muted alert to be
  // active since the tab was already muted.
  SimulateAudioState(true);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(), TabAlert::AUDIO_MUTING);

  // Turning off the audio state shouldn't immediately deactivate the muted
  // alert since the tab is still recently audible.
  SimulateAudioState(false);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(), TabAlert::AUDIO_MUTING);

  // After waiting until the tab is no longer recently audible, the muted alert
  // state should go away.
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}
}  // namespace tabs
