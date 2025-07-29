// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
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
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

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

#if BUILDFLAG(ENABLE_GLIC)
class TestGlicKeyedService : public glic::GlicKeyedService {
 public:
  TestGlicKeyedService(
      content::BrowserContext* browser_context,
      signin::IdentityManager* identity_manager,
      ProfileManager* profile_manager,
      glic::GlicProfileManager* glic_profile_manager,
      contextual_cueing::ContextualCueingService* contextual_cueing_service)
      : GlicKeyedService(Profile::FromBrowserContext(browser_context),
                         identity_manager,
                         profile_manager,
                         glic_profile_manager,
                         contextual_cueing_service) {}
};
#endif  // BUILDFLAG(ENABLE_GLIC)

class TabAlertControllerTest : public testing::Test {
 public:
  void SetUp() override {
#if BUILDFLAG(ENABLE_GLIC)
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton,
         glic::mojom::features::kGlicMultiTab},
        {});
#endif  // BUILDFLAG(ENABLE_GLIC)

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
    profile_ = testing_profile_manager_->CreateTestingProfile("profile");

#if BUILDFLAG(ENABLE_GLIC)
    test_glic_keyed_service_ = std::make_unique<TestGlicKeyedService>(
        profile_, identity_test_environment.identity_manager(),
        testing_profile_manager_->profile_manager(), &glic_profile_manager_,
        /*contextual_cueing_service=*/nullptr);
    glic::ForceSigninAndModelExecutionCapability(profile_);
#endif  // BUILDFLAG(ENABLE_GLIC)

    browser_window_interface_ =
        std::make_unique<FakeBrowserWindowInterface>(profile_);
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_delegate_->SetBrowserWindowInterface(
        browser_window_interface_.get());
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), profile_);

    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
    tab_model_ = std::make_unique<TabModel>(std::move(web_contents),
                                            tab_strip_model_.get());

#if BUILDFLAG(ENABLE_GLIC)
    tab_alert_controller_ = std::make_unique<TabAlertController>(
        *tab_model_.get(), test_glic_keyed_service_.get());
#else
    tab_alert_controller_ =
        std::make_unique<TabAlertController>(*tab_model_.get());
#endif  // BUILDFLAG(ENABLE_GLIC)
  }

  void TearDown() override {
    // Explicitly reset the pointers to prevent them from causing the
    // BrowserTaskEnvironment to time out on destruction.
    tab_alert_controller_.reset();
    tab_model_.reset();
    tab_strip_model_.reset();
    tab_strip_model_delegate_.reset();
    browser_window_interface_.reset();
#if BUILDFLAG(ENABLE_GLIC)
    test_glic_keyed_service_.reset();
#endif  // BUILDFLAG(ENABLE_GLIC)
    profile_ = nullptr;
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();
    testing_profile_manager_.reset();
  }

  TabAlertController* tab_alert_controller() {
    return tab_alert_controller_.get();
  }

  TabInterface* tab_interface() { return tab_model_.get(); }

  void SimulateAudioState(bool is_playing_audio) {
    content::WebContentsTester::For(tab_model_->GetContents())
        ->SetIsCurrentlyAudible(is_playing_audio);
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

#if BUILDFLAG(ENABLE_GLIC)
  TestGlicKeyedService* test_glic_keyed_service() {
    return test_glic_keyed_service_.get();
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler test_enabler_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  signin::IdentityTestEnvironment identity_test_environment;

#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicProfileManager glic_profile_manager_;
  std::unique_ptr<TestGlicKeyedService> test_glic_keyed_service_;
#endif  // BUILDFLAG(ENABLE_GLIC)

  std::unique_ptr<FakeBrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<TabModel> tab_model_;
  std::unique_ptr<TabAlertController> tab_alert_controller_;
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

#if BUILDFLAG(ENABLE_GLIC)
TEST_F(TabAlertControllerTest, GlicSharingUpdatesAlertController) {
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
  glic::GlicSharingManager& glic_sharing_manager =
      test_glic_keyed_service()->sharing_manager();
  glic_sharing_manager.PinTabs({tab_interface()->GetHandle()});
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::GLIC_SHARING);
  glic_sharing_manager.UnpinAllTabs();
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}
#endif  // BUILDFLAG(ENABLE_GLIC)
}  // namespace tabs
