// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_SELECTION_NOTIFICATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_SELECTION_NOTIFICATION_HANDLER_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_id.h"
#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"
#include "chromeos/ash/components/audio/device_activate_type.h"
#include "ui/message_center/message_center.h"

namespace ash {

// A callback of CrasAudioHandler::SwitchToDevice function, used to handle the
// switch button on audio selection notification being clicked.
using SwitchToDeviceCallback =
    base::RepeatingCallback<void(const AudioDevice& device,
                                 bool notify,
                                 DeviceActivateType activate_by)>;

// A callback function to open OS Settings audio page.
using OpenSettingsAudioPageCallback = base::RepeatingCallback<void()>;

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

  // Time delta to debounce the audio selection notification.
  static constexpr base::TimeDelta kDebounceTime = base::Milliseconds(1500);

  // The audio selection notification id, used to identify the notification
  // itself.
  static constexpr char kAudioSelectionNotificationId[] =
      "audio_selection_notification";

  // The audio selection notifier id.
  static constexpr char kAudioSelectionNotifierId[] =
      "ash.audio_selection_notification_handler";

  // Different types of audio selection notification.
  // Do not reorder since it's used in histogram metrics.
  enum class NotificationType {
    // A single audio device source with only input audio device. e.g. a web
    // cam.
    kSingleSourceWithInputOnly = 0,
    // A single audio device source with only output audio device. e.g. a HDMI
    // display.
    kSingleSourceWithOutputOnly = 1,
    // A single audio device source with both input and output audio devices.
    // e.g. a USB audio device.
    kSingleSourceWithInputAndOutput = 2,
    // Multiple audio device sources, e.g. a web cam and a HDMI display.
    kMultipleSources = 3,
    kMaxValue = kMultipleSources,
  };

  // Stores minimal info to create a notification. The device names will be
  // displayed in notification body. If the notification type is
  // kMultipleSources, device name refers to currently activated device name.
  // Otherwise, device name refers to hot plugged device name.
  struct NotificationTemplate {
    NotificationTemplate(NotificationType type,
                         std::optional<std::string> input_device_name,
                         std::optional<std::string> output_device_name);
    ~NotificationTemplate();

    NotificationType type;
    std::optional<std::string> input_device_name;
    std::optional<std::string> output_device_name;
  };

  // Creates and displays an audio selection notification to let users make the
  // switching or not switching decision.
  // TODO(b/333608911): Revisit audio selection notification after updated audio
  // sliders in quick settings.
  void ShowAudioSelectionNotification(
      const AudioDeviceList& hotplug_input_devices,
      const AudioDeviceList& hotplug_output_devices,
      const std::optional<std::string>& active_input_device_name,
      const std::optional<std::string>& active_output_device_name,
      SwitchToDeviceCallback switch_to_device_callback,
      OpenSettingsAudioPageCallback open_settings_audio_page_callback);

  // Handles the situation when a hotplugged device which triggers the
  // notification has been activated by users via settings or quick settings,
  // rather than via the switch button on notification body. Remove the
  // notification in this case.
  void RemoveNotificationIfHotpluggedDeviceActivated(
      const AudioDeviceList& activated_devices);

  // Handles the situation when a hotplugged device which triggers the
  // notification has been removed. Remove the notification in this case.
  void RemoveNotificationIfHotpluggedDeviceDisconnected(
      bool is_input,
      const AudioDeviceList& current_devices);

 private:
  // Grant friend access for comprehensive testing of private/protected members.
  friend class AudioSelectionNotificationHandlerTest;

  // Handles when the switch button is clicked.
  void HandleSwitchButtonClicked(
      const AudioDeviceList& devices_to_activate,
      SwitchToDeviceCallback switch_to_device_callback,
      NotificationType notification_type,
      std::optional<int> button_index);

  // Handles when the settings button is clicked. |open_settigns_callback| is
  // the callback to open the system settings audio page. |button_index|
  // indicates the index of the button on notification body that is clicked.
  void HandleSettingsButtonClicked(
      OpenSettingsAudioPageCallback open_settigns_callback,
      std::optional<int> button_index);

  // Checks if one audio input device and one audio output device belong to the
  // same physical audio device.
  bool AudioNodesBelongToSameSource(const AudioDevice& input_device,
                                    const AudioDevice& output_device);

  // Gets necessary information to create and display notitification, such as
  // notitication type and device name.
  NotificationTemplate GetNotificationTemplate(
      const AudioDeviceList& hotplug_input_devices,
      const AudioDeviceList& hotplug_output_devices,
      const std::optional<std::string>& active_input_device_name,
      const std::optional<std::string>& active_output_device_name);

  // A helper function to determine notification type and show notification.
  void ShowNotification(
      const std::optional<std::string>& active_input_device_name,
      const std::optional<std::string>& active_output_device_name,
      SwitchToDeviceCallback switch_to_device_callback,
      OpenSettingsAudioPageCallback open_settings_audio_page_callback);

  // Handles firing of audio selection related metrics.
  AudioDeviceMetricsHandler audio_device_metrics_handler_;

  // The hotplugged devices that trigger the notification. Clear the list if the
  // notification is removed.
  AudioDeviceList hotplug_input_devices_;
  AudioDeviceList hotplug_output_devices_;

  // Used to debounce the notification.
  base::RetainingOneShotTimer show_notification_debounce_timer_;

  base::WeakPtrFactory<AudioSelectionNotificationHandler> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_SELECTION_NOTIFICATION_HANDLER_H_
