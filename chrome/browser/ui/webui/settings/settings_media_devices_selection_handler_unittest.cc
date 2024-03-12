// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
MATCHER(AudioDeviceEq, "") {
  const auto& actual_device = std::get<0>(arg).GetDict();
  const auto& expected_device = std::get<1>(arg);

  return expected_device.unique_id == *actual_device.FindString("id") &&
         expected_device.device_name == *actual_device.FindString("name");
}

MATCHER(VideoDeviceEq, "") {
  const auto& actual_device = std::get<0>(arg).GetDict();
  const auto& expected_device = std::get<1>(arg);

  return expected_device.device_id == *actual_device.FindString("id") &&
         expected_device.display_name() == *actual_device.FindString("name");
}

constexpr char kMic[] = "mic";
constexpr char kCamera[] = "camera";
}  // namespace

namespace settings {

class MediaDevicesSelectionHandlerTest
    : public testing::Test,
      public content::TestWebUI::JavascriptCallObserver {
 public:
  void SetUp() override {
    auto_reset_audio_service_.emplace(
        content::OverrideAudioServiceForTesting(&fake_audio_service_));
    content::OverrideVideoCaptureServiceForTesting(
        &fake_video_capture_service_);
    auto_reset_media_device_info_override_.emplace(
        media_effects::MediaDeviceInfo::OverrideInstanceForTesting());
    handler_.emplace(&profile_);
    handler_->SetWebUiForTest(&test_web_ui_);
    handler_->RegisterMessages();
    EXPECT_TRUE(test_web_ui_.call_data().empty());
    handler_->AllowJavascriptForTesting();
    test_web_ui_.ClearTrackedCalls();
    test_web_ui_.AddJavascriptCallObserver(this);
  }

  void TearDown() override {
    content::OverrideVideoCaptureServiceForTesting(nullptr);
  }

  void VerifyUpdateDevicesMenu(
      const std::vector<media::AudioDeviceDescription>& expected_devices,
      const media::AudioDeviceDescription& expected_default_device) {
    ASSERT_EQ(1u, test_web_ui_.call_data().size());
    auto& last_call_data = *(test_web_ui_.call_data().back());
    EXPECT_EQ("cr.webUIListenerCallback", last_call_data.function_name());
    EXPECT_EQ("updateDevicesMenu", last_call_data.arg1()->GetString());
    EXPECT_EQ(kMic, last_call_data.arg2()->GetString());
    EXPECT_THAT(last_call_data.arg3()->GetList(),
                testing::Pointwise(AudioDeviceEq(), expected_devices));
    EXPECT_EQ(expected_default_device.unique_id,
              last_call_data.arg4()->GetString());
    test_web_ui_.ClearTrackedCalls();
  }

  void VerifyUpdateDevicesMenu(
      const std::vector<media::VideoCaptureDeviceDescriptor>& expected_devices,
      const media::VideoCaptureDeviceDescriptor& expected_default_device) {
    ASSERT_EQ(1u, test_web_ui_.call_data().size());
    auto& last_call_data = *(test_web_ui_.call_data().back());
    EXPECT_EQ("cr.webUIListenerCallback", last_call_data.function_name());
    EXPECT_EQ("updateDevicesMenu", last_call_data.arg1()->GetString());
    EXPECT_EQ(kCamera, last_call_data.arg2()->GetString());
    EXPECT_THAT(last_call_data.arg3()->GetList(),
                testing::Pointwise(VideoDeviceEq(), expected_devices));
    EXPECT_EQ(expected_default_device.device_id,
              last_call_data.arg4()->GetString());
    test_web_ui_.ClearTrackedCalls();
  }

  bool WaitForUpdateDevicesMenuCall(const std::string& type) {
    auto result = on_update_devices_menu_future_.WaitAndClear();
    if (result && !test_web_ui_.call_data().empty()) {
      const auto updated_type =
          test_web_ui_.call_data().back()->arg2()->GetString();
      // Got an update for the other type. Try again.
      if (type != updated_type) {
        test_web_ui_.ClearTrackedCalls();
        result = on_update_devices_menu_future_.WaitAndClear();
      }
    }

    return result;
  }

  void SendSetPreferredCaptureDeviceMessage(const std::string& type,
                                            const std::string id) {
    base::Value::List setDefaultArgs;
    setDefaultArgs.Append(type);
    setDefaultArgs.Append(id);
    test_web_ui_.ProcessWebUIMessage(GURL(), "setPreferredCaptureDevice",
                                     std::move(setDefaultArgs));
  }

  void SendInitializeCaptureDevicesMessage(const std::string& type) {
    base::Value::List getDefaultArgs;
    getDefaultArgs.Append(kMic);
    test_web_ui_.ProcessWebUIMessage(GURL(), "initializeCaptureDevices",
                                     std::move(getDefaultArgs));
  }

 protected:
  content::TestWebUI test_web_ui_;
  media_effects::FakeAudioService fake_audio_service_;
  media_effects::FakeVideoCaptureService fake_video_capture_service_;

 private:
  void OnJavascriptFunctionCalled(
      const content::TestWebUI::CallData& call_data) override {
    const std::string* function_name = call_data.arg1()->GetIfString();
    if (function_name && *function_name == "updateDevicesMenu") {
      on_update_devices_menu_future_.GetCallback().Run();
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  base::SystemMonitor system_monitor_;
  TestingProfile profile_;

  base::test::TestFuture<void> on_update_devices_menu_future_;
  std::optional<base::AutoReset<audio::mojom::AudioService*>>
      auto_reset_audio_service_;
  // When `MediaDeviceInfo::OverrideInstanceForTesting()` is called it returns
  // an `AutoReset` that removes the override when it's destructed.
  // std::optional is used to hold it because we don't get the value until
  // `SetUp()` and `AutoReset` doesn't have a default constructor.
  std::optional<std::pair<std::unique_ptr<media_effects::MediaDeviceInfo>,
                          base::AutoReset<media_effects::MediaDeviceInfo*>>>
      auto_reset_media_device_info_override_;
  std::optional<MediaDevicesSelectionHandler> handler_;
};

TEST_F(MediaDevicesSelectionHandlerTest, InitializeCaptureDevices_Mic) {
  const media::AudioDeviceDescription kDefaultDevice{
      /*device_name=*/"Default",
      /*unique_id=*/media::AudioDeviceDescription::kDefaultDeviceId,
      /*group_id=*/"default_group_id",
      /*is_system_default=*/true};
  const media::AudioDeviceDescription kIntegratedDevice{
      /*device_name=*/"Integrated Device",
      /*unique_id=*/"integrated_device",
      /*group_id=*/"integrated_group_id",
      /*is_system_default=*/true};
  const media::AudioDeviceDescription kExpectedIntegratedDevice{
      /*device_name=*/"Integrated Device (System default)",
      /*unique_id=*/"integrated_device",
      /*group_id=*/"integrated_group_id",
      /*is_system_default=*/true};
  const media::AudioDeviceDescription kUsbDevice{/*device_name=*/"USB Device",
                                                 /*unique_id=*/"usb_device",
                                                 /*group_id=*/"usb_group_id"};

  for (const auto& device : {kDefaultDevice, kIntegratedDevice, kUsbDevice}) {
    fake_audio_service_.AddFakeInputDevice(device);
  }

  ASSERT_TRUE(WaitForUpdateDevicesMenuCall(kMic));

  // Verify that the list order is unmodified if pref is unset. Also verify that
  // the virtual system default is removed.
  ASSERT_NO_FATAL_FAILURE(VerifyUpdateDevicesMenu(
      {kExpectedIntegratedDevice, kUsbDevice}, kExpectedIntegratedDevice));

  SendSetPreferredCaptureDeviceMessage(kMic, kUsbDevice.unique_id);
  SendInitializeCaptureDevicesMessage(kMic);

  ASSERT_TRUE(WaitForUpdateDevicesMenuCall(kMic));
  // Verify that the previously set preferred device is at the beginning of the
  // list.
  ASSERT_NO_FATAL_FAILURE(VerifyUpdateDevicesMenu(
      {kUsbDevice, kExpectedIntegratedDevice}, kUsbDevice));
}

// Verify that the virtual system default device stays in the list when we can't
// map to the real system default. Note that this is only for micrphones as
// cameras don't have system defaults.
TEST_F(MediaDevicesSelectionHandlerTest,
       InitializeCaptureDevices_Mic_KeepVirtualSystemDefault) {
  const media::AudioDeviceDescription kDefaultDevice{
      /*device_name=*/"Default",
      /*unique_id=*/media::AudioDeviceDescription::kDefaultDeviceId,
      /*group_id=*/"default_group_id",
      /*is_system_default=*/true};
  const media::AudioDeviceDescription kExpectedDefaultDevice{
      /*device_name=*/"System default",
      /*unique_id=*/media::AudioDeviceDescription::kDefaultDeviceId,
      /*group_id=*/"default_group_id",
      /*is_system_default=*/true};
  const media::AudioDeviceDescription kIntegratedDevice{
      /*device_name=*/"Integrated Device",
      /*unique_id=*/"integrated_device",
      /*group_id=*/"integrated_group_id"};
  const media::AudioDeviceDescription kUsbDevice{/*device_name=*/"USB Device",
                                                 /*unique_id=*/"usb_device",
                                                 /*group_id=*/"usb_group_id"};

  std::vector<media::AudioDeviceDescription> devices{
      kDefaultDevice, kIntegratedDevice, kUsbDevice};

  for (const auto& device : devices) {
    fake_audio_service_.AddFakeInputDevice(device);
  }

  ASSERT_TRUE(WaitForUpdateDevicesMenuCall(kMic));

  // Verify that the list order is unmodified if pref is unset.
  ASSERT_NO_FATAL_FAILURE(VerifyUpdateDevicesMenu(
      {kExpectedDefaultDevice, kIntegratedDevice, kUsbDevice},
      kExpectedDefaultDevice));

  SendSetPreferredCaptureDeviceMessage(kMic, kUsbDevice.unique_id);
  SendInitializeCaptureDevicesMessage(kMic);

  ASSERT_TRUE(WaitForUpdateDevicesMenuCall(kMic));
  // Verify that the previously set preferred device is at the beginning of the
  // list.
  ASSERT_NO_FATAL_FAILURE(VerifyUpdateDevicesMenu(
      {kUsbDevice, kExpectedDefaultDevice, kIntegratedDevice}, kUsbDevice));
}

TEST_F(MediaDevicesSelectionHandlerTest, InitializeCaptureDevices_Camera) {
  const media::VideoCaptureDeviceDescriptor kIntegratedDevice{
      /*display_name=*/"Integrated Device",
      /*device_id=*/"integrated_device",
  };
  const media::VideoCaptureDeviceDescriptor kUsbDevice{
      /*display_name=*/"USB Device",
      /*device_id=*/"usb_device",
  };

  std::vector<media::VideoCaptureDeviceDescriptor> devices{kIntegratedDevice,
                                                           kUsbDevice};

  for (const auto& device : devices) {
    fake_video_capture_service_.AddFakeCamera(device);
  }

  ASSERT_TRUE(WaitForUpdateDevicesMenuCall(kCamera));

  // Verify that the list order is unmodified if pref is unset.
  ASSERT_NO_FATAL_FAILURE(VerifyUpdateDevicesMenu(devices, kIntegratedDevice));

  SendSetPreferredCaptureDeviceMessage(kCamera, kUsbDevice.device_id);
  SendInitializeCaptureDevicesMessage(kCamera);

  ASSERT_TRUE(WaitForUpdateDevicesMenuCall(kCamera));
  // Verify that the previously set preferred device is at the beginning of the
  // list.
  ASSERT_NO_FATAL_FAILURE(
      VerifyUpdateDevicesMenu({kUsbDevice, kIntegratedDevice}, kUsbDevice));
}

}  // namespace settings
