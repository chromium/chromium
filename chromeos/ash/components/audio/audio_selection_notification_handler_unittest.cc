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

}  // namespace ash
