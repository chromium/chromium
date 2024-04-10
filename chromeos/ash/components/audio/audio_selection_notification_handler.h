// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_SELECTION_NOTIFICATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_SELECTION_NOTIFICATION_HANDLER_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_id.h"
#include "ui/message_center/message_center.h"

namespace ash {

// AudioSelectionNotificationHandler handles the creation and display of the
// audio selection notification.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO)
    AudioSelectionNotificationHandler {
 public:
  AudioSelectionNotificationHandler();
  AudioSelectionNotificationHandler(const AudioSelectionNotificationHandler&) =
      delete;
  AudioSelectionNotificationHandler& operator=(
      const AudioSelectionNotificationHandler&) = delete;
  ~AudioSelectionNotificationHandler();

  // Creates and displays an audio selection notification to let users make the
  // switching or not switching decision.
  // TODO(b/333608911): Revisit audio selection notification after updated audio
  // sliders in quick settings.
  void ShowAudioSelectionNotification(
      const AudioDeviceList& hotplug_input_devices,
      const AudioDeviceList& hotplug_output_devices,
      const std::optional<std::string>& active_input_device_name,
      const std::optional<std::string>& active_output_device_name);

 private:
  // Handles when the notification button is clicked.
  void HandleNotificationClicked(bool is_settings_button,
                                 const AudioDeviceList& devices_to_activate,
                                 std::optional<int> button_index);

  base::WeakPtrFactory<AudioSelectionNotificationHandler> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_SELECTION_NOTIFICATION_HANDLER_H_
