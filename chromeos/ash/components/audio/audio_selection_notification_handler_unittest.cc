// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_selection_notification_handler.h"

#include <optional>

#include "ash/strings/grit/ash_strings.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

class AudioSelectionNotificationHandlerTest
    : public AudioDeviceSelectionTestBase {
 public:
  void SetUp() override { message_center::MessageCenter::Initialize(); }

  void TearDown() override { message_center::MessageCenter::Shutdown(); }

  AudioSelectionNotificationHandler& audio_selection_notification_handler() {
    return audio_selection_notification_handler_;
  }

  bool AudioNodesBelongToSameSource(const AudioDevice& input_device,
                                    const AudioDevice& output_device) {
    return audio_selection_notification_handler_.AudioNodesBelongToSameSource(
        input_device, output_device);
  }

  // Gets the count of audio selection notification.
  size_t GetNotificationCount() {
    auto* message_center = message_center::MessageCenter::Get();
    return message_center->NotificationCount();
  }

  // Gets the title of audio selection notification. If not found, return
  // std::nullopt.
  const std::optional<std::u16string> GetNotificationTitle() {
    auto* message_center = message_center::MessageCenter::Get();
    message_center::Notification* notification =
        message_center->FindNotificationById(
            AudioSelectionNotificationHandler::kAudioSelectionNotificationId);
    return notification ? std::make_optional(notification->title())
                        : std::nullopt;
  }

  // Gets the message of audio selection notification. If not found, return
  // std::nullopt.
  const std::optional<std::u16string> GetNotificationMessage() {
    auto* message_center = message_center::MessageCenter::Get();
    message_center::Notification* notification =
        message_center->FindNotificationById(
            AudioSelectionNotificationHandler::kAudioSelectionNotificationId);
    return notification ? std::make_optional(notification->message())
                        : std::nullopt;
  }

 private:
  AudioSelectionNotificationHandler audio_selection_notification_handler_;
};

TEST_F(AudioSelectionNotificationHandlerTest, ShowAudioSelectionNotification) {
  EXPECT_EQ(0u, GetNotificationCount());

  AudioDeviceList hotplug_input_devices = {
      AudioDevice(NewInputNode("INTERNAL_MIC"))};
  AudioDeviceList hotplug_output_devices = {
      AudioDevice(NewOutputNode("INTERNAL_SPEAKER"))};

  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt,
      std::nullopt);

  // Expect notification is shown.
  EXPECT_EQ(1u, GetNotificationCount());

  // Expect new notification to replace the old one and the current notification
  // count does not change.
  hotplug_input_devices = {AudioDevice(NewInputNode("MIC"))};
  hotplug_output_devices = {AudioDevice(NewOutputNode("HEADPHONE"))};
  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt,
      std::nullopt);
  EXPECT_EQ(1u, GetNotificationCount());
}

// Tests that AudioNodesBelongToSameSource can tell if one audio input device
// and one audio output device belong to the same physical audio device.
TEST_F(AudioSelectionNotificationHandlerTest, AudioNodesBelongToSameSource) {
  struct {
    const AudioDevice input_device;
    const AudioDevice output_device;
    bool same_source;
  } items[] = {
      {AudioDevice(NewInputNode("INTERNAL_MIC")),
       AudioDevice(NewOutputNode("INTERNAL_SPEAKER")), true},
      {AudioDevice(NewInputNode("FRONT_MIC")),
       AudioDevice(NewOutputNode("INTERNAL_SPEAKER")), true},
      {AudioDevice(NewInputNode("REAR_MIC")),
       AudioDevice(NewOutputNode("INTERNAL_SPEAKER")), true},
      {AudioDevice(NewInputNode("MIC")),
       AudioDevice(NewOutputNode("HEADPHONE")), true},
      {AudioDevice(NewInputNode("BLUETOOTH_NB_MIC")),
       AudioDevice(NewOutputNode("BLUETOOTH")), true},
      {AudioDevice(NewNodeWithName(/*is_input=*/true, "USB",
                                   "Razer USB Sound Card: USB Audio:2,0: Mic")),
       AudioDevice(
           NewNodeWithName(/*is_input=*/false, "USB",
                           "Razer USB Sound Card: USB Audio:2,0: Speaker")),
       true},
      {AudioDevice(NewNodeWithName(/*is_input=*/true, "BLUETOOTH", "Airpods")),
       AudioDevice(NewNodeWithName(/*is_input=*/false, "BLUETOOTH", "Airpods")),
       true},
      // Audio devices with different types do not belong to the same physical
      // device.
      {AudioDevice(NewInputNode("INTERNAL_MIC")),
       AudioDevice(NewOutputNode("HEADPHONE")), false},
      // Audio devices with different types do not belong to the same physical
      // device.
      {AudioDevice(NewInputNode("BLUETOOTH")),
       AudioDevice(NewOutputNode("HDMI")), false},
      // Audio devices with different types do not belong to the same physical
      // device.
      {AudioDevice(NewInputNode("USB")), AudioDevice(NewOutputNode("HDMI")),
       false},
      // Audio devices with different types do not belong to the same physical
      // device.
      {AudioDevice(NewInputNode("BLUETOOTH")),
       AudioDevice(NewOutputNode("USB")), false},
      // Audio devices with different device source names do not belong to the
      // same physical device.
      {AudioDevice(
           NewNodeWithName(/*is_input=*/true, "BLUETOOTH", "Airpods Pro")),
       AudioDevice(NewNodeWithName(/*is_input=*/false, "BLUETOOTH", "Airpods")),
       false},
      // Audio devices with different device source names do not belong to the
      // same physical device.
      {AudioDevice(NewNodeWithName(/*is_input=*/true, "USB",
                                   "Razer USB Sound Card: USB Audio:2,0: Mic")),
       AudioDevice(NewNodeWithName(/*is_input=*/false, "USB",
                                   "CS201 USB AUDIO: USB Audio:2,0: PCM")),
       false},
  };

  for (const auto& item : items) {
    EXPECT_EQ(item.same_source, AudioNodesBelongToSameSource(
                                    item.input_device, item.output_device));
  }
}

// Tests audio selection notification with input only displays correctly.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_SingleSourceWithInputOnly) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a web cam input.
  const std::string input_device_name =
      "HD Pro Webcam C920: USB Audio:2,0: Mic";
  AudioDeviceList hotplug_input_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/true, "USB", input_device_name))};
  AudioDeviceList hotplug_output_devices = {};
  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt,
      std::nullopt);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_OR_OUTPUT_BODY,
                base::UTF8ToUTF16(input_device_name)),
            message.value());
}

// Tests audio selection notification with output only displays correctly.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_SingleSourceWithOutputOnly) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug HDMI display with audio output.
  AudioDeviceList hotplug_input_devices = {};
  const std::string output_device_name = "Sceptre Z27";
  AudioDeviceList hotplug_output_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/false, "HDMI", output_device_name))};
  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt,
      std::nullopt);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SWITCH_OUTPUT_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_OR_OUTPUT_BODY,
                base::UTF8ToUTF16(output_device_name)),
            message.value());
}

// Tests audio selection notification with single source and both input and
// output displays correctly.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_SingleSourceWithBothInputAndOutput) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a USB input and a USB output device from the same source.
  const std::string device_source_name = "Razer USB Sound Card";
  const std::string input_device_name =
      device_source_name + ": USB Audio:2,0: Mic";
  AudioDeviceList hotplug_input_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/true, "USB", input_device_name))};
  const std::string output_device_name =
      device_source_name + ": USB Audio:2,0: Speaker";
  AudioDeviceList hotplug_output_devices = {AudioDevice(
      NewNodeWithName(/*is_input=*/false, "USB", output_device_name))};
  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt,
      std::nullopt);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SWITCH_SOURCE_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_AND_OUTPUT_BODY,
                base::UTF8ToUTF16(device_source_name)),
            message.value());
}

// Tests audio selection notification with multiple audio sources displays
// correctly.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_MultipleSources_SameAudioTypes) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a USB input and a USB output device from different sources.
  const std::string input_device_name = "CS201 USB AUDIO: USB Audio:2,0: Mic";
  AudioDeviceList hotplug_input_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/true, "USB", input_device_name))};
  const std::string output_device_name =
      "Razer USB Sound Card: USB Audio:2,0: Speaker";
  AudioDeviceList hotplug_output_devices = {AudioDevice(
      NewNodeWithName(/*is_input=*/false, "USB", output_device_name))};
  const std::string current_active_input = "internal_mic";
  const std::string current_active_output = "internal_speaker";
  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, current_active_input,
      current_active_output);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_BODY,
                                 base::UTF8ToUTF16(current_active_input),
                                 base::UTF8ToUTF16(current_active_output)),
      message.value());
}

// Tests audio selection notification with multiple audio sources displays
// correctly.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_MultipleSources_DifferentAudioTypes) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a USB input and a USB output device from different sources.
  const std::string input_device_name =
      "HD Pro Webcam C920: USB Audio:2,0: Mic";
  AudioDeviceList hotplug_input_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/true, "USB", input_device_name))};
  const std::string output_device_name = "Sceptre Z27";
  AudioDeviceList hotplug_output_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/false, "HDMI", output_device_name))};
  const std::string current_active_input = "internal_mic";
  const std::string current_active_output = "internal_speaker";
  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, current_active_input,
      current_active_output);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_BODY,
                                 base::UTF8ToUTF16(current_active_input),
                                 base::UTF8ToUTF16(current_active_output)),
      message.value());
}

}  // namespace ash
