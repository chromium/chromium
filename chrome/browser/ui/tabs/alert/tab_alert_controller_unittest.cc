// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"

#include <memory>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/alert/child_tab_alert_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_alert.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "media/mojo/mojom/display_media_information.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace tabs {

class MockTabAlertControllerSubscriber {
 public:
  MockTabAlertControllerSubscriber() = default;

  MOCK_METHOD1(OnPrioritizedAlertStateChanged,
               void(std::optional<TabAlert> new_alert));
};

class TabAlertControllerTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);

    mock_tab_ = std::make_unique<MockTabInterface>();
    ON_CALL(*mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents_.get()));
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));

    vr_tab_helper_ = std::make_unique<vr::VrTabHelper>(*mock_tab_);
    recently_audible_helper_ = std::make_unique<RecentlyAudibleHelper>(
        *mock_tab_, web_contents_.get());
    child_tab_alert_helper_ = std::make_unique<ChildTabAlertHelper>(*mock_tab_);
    tab_alert_controller_ = std::make_unique<TabAlertController>(*mock_tab_);
  }

  void TearDown() override {
    // Explicitly reset the pointers to prevent them from causing the
    // BrowserTaskEnvironment to time out on destruction.
    tab_alert_controller_.reset();
    child_tab_alert_helper_.reset();
    recently_audible_helper_.reset();
    vr_tab_helper_.reset();
    mock_tab_.reset();
    web_contents_.reset();
    profile_.reset();
  }

  TabAlertController* tab_alert_controller() {
    return tab_alert_controller_.get();
  }

  ChildTabAlertHelper* child_tab_alert_helper() {
    return ChildTabAlertHelper::From(mock_tab_.get());
  }

  TabInterface* tab_interface() { return mock_tab_.get(); }

  void SimulateAudioState(bool is_playing_audio) {
    content::WebContentsTester::For(web_contents_.get())
        ->SetIsCurrentlyAudible(is_playing_audio);
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler test_enabler_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MockTabInterface> mock_tab_;
  std::unique_ptr<vr::VrTabHelper> vr_tab_helper_;
  std::unique_ptr<RecentlyAudibleHelper> recently_audible_helper_;
  std::unique_ptr<ChildTabAlertHelper> child_tab_alert_helper_;
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
                  std::make_optional(TabAlert::kAudioPlaying)));
  SimulateAudioState(true);
  ::testing::Mock::VerifyAndClearExpectations(mock_subscriber.get());

  // Simulate a higher priority alert being activated.
  EXPECT_CALL(*mock_subscriber, OnPrioritizedAlertStateChanged(
                                    std::make_optional(TabAlert::kPipPlaying)));
  tab_alert_controller()->MediaPictureInPictureChanged(true);
  ::testing::Mock::VerifyAndClearExpectations(mock_subscriber.get());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(), TabAlert::kPipPlaying);

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
  EXPECT_EQ(prioritized_alert.value(), TabAlert::kBluetoothConnected);
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 4U);

  // Verify that the active alerts list is in sorted order
  std::vector<TabAlert> active_alerts =
      tab_alert_controller()->GetAllActiveAlerts();
  EXPECT_EQ(active_alerts[0], TabAlert::kBluetoothConnected);
  EXPECT_EQ(active_alerts[1], TabAlert::kPipPlaying);
  EXPECT_EQ(active_alerts[2], TabAlert::kAudioMuting);
  EXPECT_EQ(active_alerts[3], TabAlert::kAudioPlaying);
}

TEST_F(TabAlertControllerTest, AlertIsActive) {
  SimulateAudioState(true);
  tab_alert_controller()->OnCapabilityTypesChanged(
      content::WebContentsCapabilityType::kBluetoothConnected, true);
  tab_alert_controller()->MediaPictureInPictureChanged(true);

  EXPECT_TRUE(tab_alert_controller()->IsAlertActive(TabAlert::kAudioPlaying));
  EXPECT_TRUE(
      tab_alert_controller()->IsAlertActive(TabAlert::kBluetoothConnected));
  EXPECT_TRUE(tab_alert_controller()->IsAlertActive(TabAlert::kPipPlaying));

  // When the non-prioritized alert is no longer active, the alert controller
  // should be updated to reflect that.
  tab_alert_controller()->MediaPictureInPictureChanged(false);
  EXPECT_FALSE(tab_alert_controller()->IsAlertActive(TabAlert::kPipPlaying));
}

TEST_F(TabAlertControllerTest, VrStateUpdatesAlertController) {
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
  vr::VrTabHelper* const vr_tab_helper = vr::VrTabHelper::From(tab_interface());
  vr_tab_helper->SetIsContentDisplayedInHeadset(true);
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::kVrPresentingInHeadset);
  vr_tab_helper->SetIsContentDisplayedInHeadset(false);
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}

TEST_F(TabAlertControllerTest, AudioStateUpdatesAlertController) {
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
  SimulateAudioState(true);
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::kAudioPlaying);

  // The audio playing alert should still be active even though the audio has
  // stopped to prevent the audio state from toggling too frequently on pause.
  SimulateAudioState(false);
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::kAudioPlaying);

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
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(), TabAlert::kAudioMuting);

  // Turning off the audio state shouldn't immediately deactivate the muted
  // alert since the tab is still recently audible.
  SimulateAudioState(false);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(), TabAlert::kAudioMuting);

  // After waiting until the tab is no longer recently audible, the muted alert
  // state should go away.
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}

TEST_F(TabAlertControllerTest, MediaStatesUpdate) {
#if BUILDFLAG(IS_CHROMEOS)
  // Need to mock the system settings to allow audio and video capture on
  // ChromeOS.
  base::AutoReset<bool> mock_system_settings =
      system_permission_settings::MockShowSystemSettingsForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS)
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());

  // Simulate audio being captured
  blink::mojom::StreamDevices audio_device;
  audio_device.audio_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "audio_device",
      "audio_device");
  auto audio_stream_ui = indicator->RegisterMediaStream(
      tab_interface()->GetContents(), audio_device);
  audio_stream_ui->OnStarted(base::DoNothing(), base::DoNothing(),
                             std::string(), {}, base::DoNothing());
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::kAudioRecording);

  // Simulate video also being captured.
  blink::mojom::StreamDevices video_device;
  video_device.video_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "video_device",
      "video_device");
  ;
  auto video_stream_ui = indicator->RegisterMediaStream(
      tab_interface()->GetContents(), video_device);
  video_stream_ui->OnStarted(base::DoNothing(), base::DoNothing(),
                             std::string(), {}, base::DoNothing());

  // The tab alert should be MEDIA_RECORDING because the tab's audio and video
  // is being captured.
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 1u);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::kMediaRecording);

  // Resetting the audio capture should leave only the video capture alert as
  // active.
  audio_stream_ui.reset();
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 1u);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::kVideoRecording);

  video_stream_ui.reset();
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}

TEST_F(TabAlertControllerTest, DesktopCapturingUpdates) {
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());

  // Simulate the display monitor being captured.
  blink::mojom::StreamDevices video_device;
  blink::MediaStreamDevice display_monitor_video_stream(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE, "video_device",
      "video_device");
  display_monitor_video_stream.display_media_info =
      media::mojom::DisplayMediaInformation::New(
          media::mojom::DisplayCaptureSurfaceType::MONITOR, true,
          media::mojom::CursorCaptureType::NEVER, nullptr, 100);
  video_device.video_device = display_monitor_video_stream;

  auto video_stream_ui = indicator->RegisterMediaStream(
      tab_interface()->GetContents(), video_device);
  video_stream_ui->OnStarted(base::DoNothing(), base::DoNothing(),
                             std::string(), {}, base::DoNothing());

  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 1u);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::kDesktopCapturing);

  // Start a second stream but capture the window instead.
  blink::mojom::StreamDevices second_video_device;
  blink::MediaStreamDevice display_window_video_stream(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE, "video_device",
      "video_device");
  display_window_video_stream.display_media_info =
      media::mojom::DisplayMediaInformation::New(
          media::mojom::DisplayCaptureSurfaceType::WINDOW, true,
          media::mojom::CursorCaptureType::NEVER, nullptr, 100);
  second_video_device.video_device = display_window_video_stream;
  auto second_video_stream_ui = indicator->RegisterMediaStream(
      tab_interface()->GetContents(), second_video_device);
  second_video_stream_ui->OnStarted(base::DoNothing(), base::DoNothing(),
                                    std::string(), {}, base::DoNothing());

  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 1u);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::kDesktopCapturing);

  // Even though the first stream has stopped, the desktop capturing alert
  // should remain active because the is still being captured by the window.
  video_stream_ui.reset();
  EXPECT_TRUE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 1u);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow().value(),
            TabAlert::kDesktopCapturing);

  // The desktop capturing alert should no longer be active after the second
  // video stream stopped.
  second_video_stream_ui.reset();
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}

TEST_F(TabAlertControllerTest, ChildMediaAlert_ActiveWhileAlive) {
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());

  base::ScopedClosureRunner child_alert =
      child_tab_alert_helper()->CreateChildMediaAlert(
          TabAlert::kVideoRecording);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kVideoRecording);
  EXPECT_TRUE(tab_alert_controller()->IsAlertActive(TabAlert::kVideoRecording));

  child_alert.RunAndReset();
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_FALSE(
      tab_alert_controller()->IsAlertActive(TabAlert::kVideoRecording));
}

TEST_F(TabAlertControllerTest, ChildMediaAlert_MultipleActiveAlerts) {
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());

  base::ScopedClosureRunner child_alert1 =
      child_tab_alert_helper()->CreateChildMediaAlert(
          TabAlert::kVideoRecording);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kVideoRecording);

  base::ScopedClosureRunner child_alert2 =
      child_tab_alert_helper()->CreateChildMediaAlert(
          TabAlert::kVideoRecording);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kVideoRecording);

  child_alert1.RunAndReset();
  EXPECT_TRUE(tab_alert_controller()->IsAlertActive(TabAlert::kVideoRecording));
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kVideoRecording);

  child_alert2.RunAndReset();
  EXPECT_FALSE(
      tab_alert_controller()->IsAlertActive(TabAlert::kVideoRecording));
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}

TEST_F(TabAlertControllerTest, ChildMediaAlert_CombinesWithTabAudio) {
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());

  // Simulate audio capture on the main tab.
  blink::mojom::StreamDevices audio_device;
  audio_device.audio_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "audio_device",
      "audio_device");
  std::unique_ptr<content::MediaStreamUI> audio_stream_ui =
      indicator->RegisterMediaStream(tab_interface()->GetContents(),
                                     audio_device);
  audio_stream_ui->OnStarted(base::DoNothing(), base::DoNothing(),
                             std::string(), {}, base::DoNothing());

  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kAudioRecording);

  // When child video recording starts while tab is capturing audio, alert
  // should combine into kMediaRecording (both audio & video).
  base::ScopedClosureRunner video_alert =
      child_tab_alert_helper()->CreateChildMediaAlert(
          TabAlert::kVideoRecording);
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 1u);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kMediaRecording);

  // Releasing the child video alert should drop back to audio recording only.
  video_alert.RunAndReset();
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 1u);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kAudioRecording);

  audio_stream_ui.reset();
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}

TEST_F(TabAlertControllerTest, ChildMediaAlert_CombinesAudioAndVideo) {
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());

  base::ScopedClosureRunner audio_alert =
      child_tab_alert_helper()->CreateChildMediaAlert(
          TabAlert::kAudioRecording);
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 1u);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kAudioRecording);

  base::ScopedClosureRunner video_alert =
      child_tab_alert_helper()->CreateChildMediaAlert(
          TabAlert::kVideoRecording);
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 1u);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kMediaRecording);

  video_alert.RunAndReset();
  EXPECT_EQ(tab_alert_controller()->GetAllActiveAlerts().size(), 1u);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kAudioRecording);

  audio_alert.RunAndReset();
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
}

TEST_F(TabAlertControllerTest, ChildMediaAlert_DirectMediaRecording) {
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());

  base::ScopedClosureRunner child_alert =
      child_tab_alert_helper()->CreateChildMediaAlert(
          TabAlert::kMediaRecording);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kMediaRecording);
  EXPECT_TRUE(tab_alert_controller()->IsAlertActive(TabAlert::kMediaRecording));

  child_alert.RunAndReset();
  EXPECT_FALSE(tab_alert_controller()->GetAlertToShow().has_value());
  EXPECT_FALSE(
      tab_alert_controller()->IsAlertActive(TabAlert::kMediaRecording));
}

TEST_F(TabAlertControllerTest, ChildMediaAlert_NotifiesObserversOnStateChange) {
  base::MockCallback<TabAlertController::AlertToShowChangedCallback>
      mock_callback;
  base::CallbackListSubscription subscription =
      tab_alert_controller()->AddAlertToShowChangedCallback(
          mock_callback.Get());

  // 0 -> 1 transition notifies observers.
  EXPECT_CALL(mock_callback, Run(testing::Optional(TabAlert::kVideoRecording)))
      .Times(1);
  base::ScopedClosureRunner child_alert_1 =
      child_tab_alert_helper()->CreateChildMediaAlert(
          TabAlert::kVideoRecording);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(),
            TabAlert::kVideoRecording);

  // 1 -> 2 transition suppresses redundant observer notification.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  base::ScopedClosureRunner child_alert_2 =
      child_tab_alert_helper()->CreateChildMediaAlert(
          TabAlert::kVideoRecording);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // 2 -> 1 transition suppresses redundant observer notification.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  child_alert_1.RunAndReset();
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // 1 -> 0 transition notifies observers.
  EXPECT_CALL(mock_callback, Run(testing::Eq(std::nullopt))).Times(1);
  child_alert_2.RunAndReset();
  testing::Mock::VerifyAndClearExpectations(&mock_callback);
  EXPECT_EQ(tab_alert_controller()->GetAlertToShow(), std::nullopt);
}

TEST_F(TabAlertControllerTest, ChildMediaAlert_SafeAfterTabDestroyed) {
  base::ScopedClosureRunner child_alert =
      child_tab_alert_helper()->CreateChildMediaAlert(
          TabAlert::kVideoRecording);
  EXPECT_TRUE(tab_alert_controller()->IsAlertActive(TabAlert::kVideoRecording));

  // Explicitly reset the controller and helper before `child_alert` goes out
  // of scope.
  tab_alert_controller_.reset();
  child_tab_alert_helper_.reset();
  child_alert.RunAndReset();
}

}  // namespace tabs
