// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_selection_notification_handler.h"

#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

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

  // Get the count of audio selection notification.
  size_t GetNotificationCount() {
    auto* message_center = message_center::MessageCenter::Get();
    return message_center->NotificationCount();
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
  hotplug_output_devices = {AudioDevice(NewOutputNode("SPEAKER"))};
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

}  // namespace ash
