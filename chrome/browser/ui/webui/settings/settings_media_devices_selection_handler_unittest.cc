// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
MATCHER(DeviceEq, "") {
  const auto& actual_device = std::get<0>(arg).GetDict();
  const auto& expected_device = std::get<1>(arg);

  return expected_device.id == *actual_device.FindString("id") &&
         expected_device.name == *actual_device.FindString("name");
}
}  // namespace

namespace settings {

class MediaDevicesSelectionHandlerTest
    : public testing::Test,
      public content::TestWebUI::JavascriptCallObserver {
 public:
  MediaDevicesSelectionHandlerTest() : handler_(&profile_) {}

  void SetUp() override {
    handler_.SetWebUiForTest(&test_web_ui_);
    handler_.RegisterMessages();
    EXPECT_TRUE(test_web_ui_.call_data().empty());
    handler_.AllowJavascriptForTesting();
    test_web_ui_.ClearTrackedCalls();
    test_web_ui_.AddJavascriptCallObserver(this);
  }

  void VerifyUpdateDevicesMenu(
      const blink::MediaStreamDevices& expected_devices,
      const blink::MediaStreamDevice& expected_default_device,
      const std::string& type) {
    ASSERT_EQ(1u, test_web_ui_.call_data().size());
    auto& last_call_data = *(test_web_ui_.call_data().back());
    EXPECT_EQ("cr.webUIListenerCallback", last_call_data.function_name());
    EXPECT_EQ("updateDevicesMenu", last_call_data.arg1()->GetString());
    EXPECT_EQ(type, last_call_data.arg2()->GetString());
    EXPECT_THAT(last_call_data.arg3()->GetList(),
                testing::Pointwise(DeviceEq(), expected_devices));
    EXPECT_EQ(expected_default_device.id, last_call_data.arg4()->GetString());
    test_web_ui_.ClearTrackedCalls();
  }

  bool WaitForUpdateDevicesMenuCall() {
    bool result = on_update_devices_menu_future_.Wait();
    on_update_devices_menu_future_.Clear();
    return result;
  }

 protected:
  content::TestWebUI test_web_ui_;

 private:
  void OnJavascriptFunctionCalled(
      const content::TestWebUI::CallData& call_data) override {
    const std::string* function_name = call_data.arg1()->GetIfString();
    if (function_name && *function_name == "updateDevicesMenu") {
      on_update_devices_menu_future_.GetCallback().Run();
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  MediaDevicesSelectionHandler handler_;
  base::test::TestFuture<void> on_update_devices_menu_future_;
};

TEST_F(MediaDevicesSelectionHandlerTest, SetDefaultAudioDevice) {
  const blink::MediaStreamDevice kIntegratedDevice(
      /*type=*/blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      /*id=*/"integrated_device",
      /*name=*/"Integrated Device");
  const blink::MediaStreamDevice kUsbDevice(
      /*type=*/blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      /*id=*/"usb_device",
      /*name=*/"USB Device");

  blink::MediaStreamDevices devices{kIntegratedDevice, kUsbDevice};

  auto* devices_dispatcher = MediaCaptureDevicesDispatcher::GetInstance();
  devices_dispatcher->SetTestAudioCaptureDevices(devices);
  devices_dispatcher->OnAudioCaptureDevicesChanged();

  ASSERT_TRUE(WaitForUpdateDevicesMenuCall());

  const std::string kMic = "mic";
  // Verify that the list order is unmodified if pref is unset.
  VerifyUpdateDevicesMenu(devices, kIntegratedDevice, kMic);

  base::Value::List setDefaultArgs;
  setDefaultArgs.Append(kMic);
  setDefaultArgs.Append(kUsbDevice.id);
  test_web_ui_.ProcessWebUIMessage(GURL(), "setDefaultCaptureDevice",
                                   std::move(setDefaultArgs));

  base::Value::List getDefaultArgs;
  getDefaultArgs.Append(kMic);
  test_web_ui_.ProcessWebUIMessage(GURL(), "getDefaultCaptureDevices",
                                   std::move(getDefaultArgs));

  ASSERT_TRUE(WaitForUpdateDevicesMenuCall());
  VerifyUpdateDevicesMenu({kUsbDevice, kIntegratedDevice}, kUsbDevice, kMic);
}

TEST_F(MediaDevicesSelectionHandlerTest, SetDefaultVideoDevice) {
  const blink::MediaStreamDevice kIntegratedDevice(
      /*type=*/blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      /*id=*/"integrated_device",
      /*name=*/"Integrated Device");
  const blink::MediaStreamDevice kUsbDevice(
      /*type=*/blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      /*id=*/"usb_device",
      /*name=*/"USB Device");

  blink::MediaStreamDevices devices{kIntegratedDevice, kUsbDevice};

  auto* devices_dispatcher = MediaCaptureDevicesDispatcher::GetInstance();
  devices_dispatcher->SetTestVideoCaptureDevices(devices);
  devices_dispatcher->OnVideoCaptureDevicesChanged();

  ASSERT_TRUE(WaitForUpdateDevicesMenuCall());

  const std::string kCamera = "camera";
  // Verify that the list order is unmodified if pref is unset.
  VerifyUpdateDevicesMenu(devices, kIntegratedDevice, kCamera);

  base::Value::List setDefaultArgs;
  setDefaultArgs.Append(kCamera);
  setDefaultArgs.Append(kUsbDevice.id);
  test_web_ui_.ProcessWebUIMessage(GURL(), "setDefaultCaptureDevice",
                                   std::move(setDefaultArgs));

  base::Value::List getDefaultArgs;
  getDefaultArgs.Append(kCamera);
  test_web_ui_.ProcessWebUIMessage(GURL(), "getDefaultCaptureDevices",
                                   std::move(getDefaultArgs));

  ASSERT_TRUE(WaitForUpdateDevicesMenuCall());
  VerifyUpdateDevicesMenu({kUsbDevice, kIntegratedDevice}, kUsbDevice, kCamera);
}

}  // namespace settings
