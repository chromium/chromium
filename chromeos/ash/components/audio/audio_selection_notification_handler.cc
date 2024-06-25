// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_selection_notification_handler.h"

#include <cstring>
#include <optional>

#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/vector_icons.h"

namespace ash {

namespace {

// Separator used to split the audio device name.
constexpr char kAudioDeviceNameSeparator[] = ":";

// Extracts the audio device source name of an audio device. e.g. the source
// name for "Razer USB Sound Card: USB Audio:2,0: Mic" would be "Razer USB Sound
// Card". The source name for "Airpods" would be "Airpods".
std::string ExtractDeviceSourceName(const AudioDevice& device) {
  std::vector<std::string> parts =
      base::SplitString(device.display_name, kAudioDeviceNameSeparator,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return parts.front();
}

// Checks if a given device is in a device list
bool IsDeviceInList(const AudioDevice& target_device,
                    const AudioDeviceList& device_list) {
  for (const AudioDevice& device : device_list) {
    if (target_device.stable_device_id == device.stable_device_id) {
      return true;
    }
  }
  return false;
}

}  // namespace

AudioSelectionNotificationHandler::AudioSelectionNotificationHandler() =
    default;
AudioSelectionNotificationHandler::~AudioSelectionNotificationHandler() =
    default;

AudioSelectionNotificationHandler::NotificationTemplate::NotificationTemplate(
    NotificationType type,
    std::optional<std::string> input_device_name,
    std::optional<std::string> output_device_name)
    : type(type),
      input_device_name(input_device_name),
      output_device_name(output_device_name) {}

AudioSelectionNotificationHandler::NotificationTemplate::
    ~NotificationTemplate() = default;

bool AudioSelectionNotificationHandler::AudioNodesBelongToSameSource(
    const AudioDevice& input_device,
    const AudioDevice& output_device) {
  CHECK(input_device.is_input);
  CHECK(!output_device.is_input);

  // Handle internal audio device.
  if ((input_device.type == AudioDeviceType::kInternalMic ||
       input_device.type == AudioDeviceType::kFrontMic ||
       input_device.type == AudioDeviceType::kRearMic) &&
      output_device.type == AudioDeviceType::kInternalSpeaker) {
    return true;
  }

  // Handle special cases where input and output device are the same source but
  // different device types. kMic and kHeadphone are the types for 3.5mm jack
  // headphone's input and output. Similarly, kBluetoothNbMic and kBluetooth are
  // the types for a bluetooth device.
  if ((input_device.type == AudioDeviceType::kMic &&
       output_device.type == AudioDeviceType::kHeadphone) ||
      (input_device.type == AudioDeviceType::kBluetoothNbMic &&
       output_device.type == AudioDeviceType::kBluetooth)) {
    return true;
  }

  // For other devices, different device types indicate different device
  // sources.
  if (input_device.type != output_device.type) {
    return false;
  }

  // With same device type, checks their device source name.
  return ExtractDeviceSourceName(input_device)
             .compare(ExtractDeviceSourceName(output_device)) == 0;
}

void AudioSelectionNotificationHandler::ShowAudioSelectionNotification(
    const AudioDeviceList& hotplug_input_devices,
    const AudioDeviceList& hotplug_output_devices,
    const std::optional<std::string>& active_input_device_name,
    const std::optional<std::string>& active_output_device_name,
    SwitchToDeviceCallback switch_to_device_callback,
    OpenSettingsAudioPageCallback open_settings_audio_page_callback) {
  // At least input or output has hotplug device.
  CHECK(!hotplug_input_devices.empty() || !hotplug_output_devices.empty());

  // If show_notification callback is already in the queue, stop it and append
  // new hot plugged devices to the existing list, so that the notification can
  // handle them together. Otherwise, reset the existing hot plugged list.
  if (show_notification_debounce_timer_.IsRunning()) {
    show_notification_debounce_timer_.Stop();
    for (const AudioDevice& device : hotplug_input_devices) {
      hotplug_input_devices_.push_back(device);
    }
    for (const AudioDevice& device : hotplug_output_devices) {
      hotplug_output_devices_.push_back(device);
    }
  } else {
    hotplug_input_devices_.clear();
    hotplug_input_devices_ = hotplug_input_devices;
    hotplug_output_devices_.clear();
    hotplug_output_devices_ = hotplug_output_devices;
  }

  show_notification_debounce_timer_.Start(
      FROM_HERE, kDebounceTime,
      base::BindRepeating(&AudioSelectionNotificationHandler::ShowNotification,
                          weak_ptr_factory_.GetWeakPtr(),
                          active_input_device_name, active_output_device_name,
                          switch_to_device_callback,
                          open_settings_audio_page_callback));
}

void AudioSelectionNotificationHandler::ShowNotification(
    const std::optional<std::string>& active_input_device_name,
    const std::optional<std::string>& active_output_device_name,
    SwitchToDeviceCallback switch_to_device_callback,
    OpenSettingsAudioPageCallback open_settings_audio_page_callback) {
  AudioDeviceList devices_to_activate;
  std::u16string title_message_id;
  std::u16string body_message_id;
  std::vector<message_center::ButtonInfo> buttons_info;
  AudioDeviceMetricsHandler::AudioSelectionNotificationEvents
      notification_event;

  NotificationTemplate notification_template = GetNotificationTemplate(
      hotplug_input_devices_, hotplug_output_devices_, active_input_device_name,
      active_output_device_name);

  // Use different notification titles and messages based on notification types.
  switch (notification_template.type) {
    case NotificationType::kSingleSourceWithInputOnly:
      title_message_id =
          l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_TITLE);
      body_message_id = l10n_util::GetStringFUTF16(
          IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_OR_OUTPUT_BODY,
          base::UTF8ToUTF16(hotplug_input_devices_.front().display_name));
      devices_to_activate.push_back(hotplug_input_devices_.front());
      buttons_info.emplace_back(
          l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_BUTTON_SWITCH));
      notification_event =
          AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
              kNotificationWithInputOnlyDeviceShowsUp;
      break;
    case NotificationType::kSingleSourceWithOutputOnly:
      title_message_id = l10n_util::GetStringUTF16(
          IDS_ASH_AUDIO_SELECTION_SWITCH_OUTPUT_TITLE);
      body_message_id = l10n_util::GetStringFUTF16(
          IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_OR_OUTPUT_BODY,
          base::UTF8ToUTF16(hotplug_output_devices_.front().display_name));
      devices_to_activate.push_back(hotplug_output_devices_.front());
      buttons_info.emplace_back(
          l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_BUTTON_SWITCH));
      notification_event =
          AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
              kNotificationWithOutputOnlyDeviceShowsUp;
      break;
    case NotificationType::kSingleSourceWithInputAndOutput:
      title_message_id = l10n_util::GetStringUTF16(
          IDS_ASH_AUDIO_SELECTION_SWITCH_SOURCE_TITLE);
      body_message_id = l10n_util::GetStringFUTF16(
          IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_AND_OUTPUT_BODY,
          base::UTF8ToUTF16(
              ExtractDeviceSourceName(hotplug_output_devices_.front())));
      devices_to_activate.push_back(hotplug_input_devices_.front());
      devices_to_activate.push_back(hotplug_output_devices_.front());
      buttons_info.emplace_back(
          l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_BUTTON_SWITCH));
      notification_event =
          AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
              kNotificationWithBothInputAndOutputDevicesShowsUp;
      break;
    case NotificationType::kMultipleSources:
      title_message_id = l10n_util::GetStringUTF16(
          IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_TITLE);
      body_message_id =
          active_input_device_name.has_value() &&
                  active_output_device_name.has_value()
              ? l10n_util::GetStringFUTF16(
                    IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_BODY,
                    base::UTF8ToUTF16(active_input_device_name.value()),
                    base::UTF8ToUTF16(active_output_device_name.value()))
              : l10n_util::GetStringUTF16(
                    IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_BODY_WITH_NAME_UNAVAILABLE);
      buttons_info.emplace_back(
          l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_BUTTON_SETTINGS));
      notification_event =
          AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
              kNotificationWithMultipleSourcesDevicesShowsUp;
      break;
  }

  audio_device_metrics_handler_.RecordNotificationEvents(notification_event);

  message_center::RichNotificationData optional_fields;
  optional_fields.buttons = buttons_info;

  // If notification type is kMultipleSources, show Settings button and pass
  // HandleSettingsButtonClicked function.
  auto notification_delegate =
      notification_template.type == NotificationType::kMultipleSources
          ? base::BindRepeating(
                &AudioSelectionNotificationHandler::HandleSettingsButtonClicked,
                weak_ptr_factory_.GetWeakPtr(),
                open_settings_audio_page_callback)
          : base::BindRepeating(
                &AudioSelectionNotificationHandler::HandleSwitchButtonClicked,
                weak_ptr_factory_.GetWeakPtr(), devices_to_activate,
                switch_to_device_callback, notification_template.type);

  message_center::Notification notification{
      /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
      /*id=*/kAudioSelectionNotificationId,
      /*title=*/title_message_id,
      /*message=*/body_message_id, ui::ImageModel(),
      /*display_source=*/
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SOURCE),
      /*origin_url=*/GURL(),
      /*notifier_id=*/
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kAudioSelectionNotifierId,
                                 NotificationCatalogName::kAudioSelection),
      optional_fields,
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          notification_delegate)};
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification.id(),
                                     /*by_user=*/false);
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(notification));
}

void AudioSelectionNotificationHandler::HandleSwitchButtonClicked(
    const AudioDeviceList& devices_to_activate,
    SwitchToDeviceCallback switch_to_device_callback,
    NotificationType notification_type,
    std::optional<int> button_index) {
  if (!button_index.has_value()) {
    // Do not do anything when notification body is clicked. If the button is
    // clicked, the button_index will have a value.
    return;
  }

  switch (notification_type) {
    case NotificationType::kSingleSourceWithInputOnly:
      audio_device_metrics_handler_.RecordNotificationEvents(
          AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
              kNotificationWithInputOnlyDeviceClicked);
      break;
    case NotificationType::kSingleSourceWithOutputOnly:
      audio_device_metrics_handler_.RecordNotificationEvents(
          AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
              kNotificationWithOutputOnlyDeviceClicked);
      break;
    case NotificationType::kSingleSourceWithInputAndOutput:
      audio_device_metrics_handler_.RecordNotificationEvents(
          AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
              kNotificationWithBothInputAndOutputDevicesClicked);
      break;
    case NotificationType::kMultipleSources:
      // Do not record in this case. When the notification type is
      // kMultipleSources, notification with settings button should display.
      NOTREACHED_IN_MIGRATION();
  }

  // Activate audio devices.
  for (const AudioDevice& device : devices_to_activate) {
    switch_to_device_callback.Run(device, /*notify=*/true,
                                  DeviceActivateType::kActivateByUser);
  }

  // Remove notification and hotplug_input/output_devices_.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(kAudioSelectionNotificationId,
                                     /*by_user=*/true);
  hotplug_input_devices_.clear();
  hotplug_output_devices_.clear();
}

void AudioSelectionNotificationHandler::HandleSettingsButtonClicked(
    base::RepeatingCallback<void()> open_settigns_callback,
    std::optional<int> button_index) {
  if (!button_index.has_value()) {
    // Do not do anything when notification body is clicked. If the button is
    // clicked, the button_index will have a value.
    return;
  }

  audio_device_metrics_handler_.RecordNotificationEvents(
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithMultipleSourcesDevicesClicked);

  // Open OS Settings audio page.
  open_settigns_callback.Run();

  // Remove notification.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(kAudioSelectionNotificationId,
                                     /*by_user=*/true);
}

AudioSelectionNotificationHandler::NotificationTemplate
AudioSelectionNotificationHandler::GetNotificationTemplate(
    const AudioDeviceList& hotplug_input_devices,
    const AudioDeviceList& hotplug_output_devices,
    const std::optional<std::string>& active_input_device_name,
    const std::optional<std::string>& active_output_device_name) {
  // There must be either hotplug input devices, or hotplug output devices, or
  // both. Otherwise, this function shouldn't be called.
  CHECK(!hotplug_input_devices.empty() || !hotplug_output_devices.empty());

  // Hot plugged devices that are used to determine notification type must be
  // simple usage audio devices.
  for (const AudioDevice& device : hotplug_input_devices) {
    CHECK(device.is_for_simple_usage());
  }
  for (const AudioDevice& device : hotplug_output_devices) {
    CHECK(device.is_for_simple_usage());
  }

  // If there are more than one input devices, or more than one output devices,
  // they must come from multiple audio sources.
  if (hotplug_input_devices.size() > 1 || hotplug_output_devices.size() > 1) {
    return {
        AudioSelectionNotificationHandler::NotificationType::kMultipleSources,
        active_input_device_name, active_output_device_name};
  }

  CHECK(hotplug_input_devices.size() <= 1 &&
        hotplug_output_devices.size() <= 1);

  // If there is exactly one input and one output device, check if they belong
  // to the same source.
  if (hotplug_input_devices.size() == 1 && hotplug_output_devices.size() == 1) {
    if (AudioSelectionNotificationHandler::AudioNodesBelongToSameSource(
            hotplug_input_devices.front(), hotplug_output_devices.front())) {
      return {AudioSelectionNotificationHandler::NotificationType::
                  kSingleSourceWithInputAndOutput,
              hotplug_input_devices.front().display_name,
              hotplug_output_devices.front().display_name};
    } else {
      return {
          AudioSelectionNotificationHandler::NotificationType::kMultipleSources,
          active_input_device_name, active_output_device_name};
    }
  }

  CHECK(hotplug_input_devices.size() + hotplug_output_devices.size() == 1);

  // If there is exactly one input device and no output device, it's
  // kSingleSourceWithInputOnly notification type.
  if (hotplug_input_devices.size() == 1) {
    return {AudioSelectionNotificationHandler::NotificationType::
                kSingleSourceWithInputOnly,
            hotplug_input_devices.front().display_name, std::nullopt};
  }

  // If there is exactly one output device and no input device, it's
  // kSingleSourceWithOutputOnly notification type.
  return {AudioSelectionNotificationHandler::NotificationType::
              kSingleSourceWithOutputOnly,
          std::nullopt, hotplug_output_devices.front().display_name};
}

void AudioSelectionNotificationHandler::
    RemoveNotificationIfHotpluggedDeviceActivated(
        const AudioDeviceList& activated_devices) {
  const AudioDeviceList& hotplug_devices = activated_devices.front().is_input
                                               ? hotplug_input_devices_
                                               : hotplug_output_devices_;
  for (const AudioDevice& device : activated_devices) {
    if (IsDeviceInList(device, hotplug_devices)) {
      // Remove notification and hotplug_input/output_devices_.
      auto* message_center = message_center::MessageCenter::Get();
      message_center->RemoveNotification(kAudioSelectionNotificationId,
                                         /*by_user=*/false);

      hotplug_input_devices_.clear();
      hotplug_output_devices_.clear();
      return;
    }
  }
}

void AudioSelectionNotificationHandler::
    RemoveNotificationIfHotpluggedDeviceDisconnected(
        bool is_input,
        const AudioDeviceList& current_devices) {
  const AudioDeviceList& hotplug_devices =
      is_input ? hotplug_input_devices_ : hotplug_output_devices_;

  // If hotplugged devices that trigger the notification does not exist in
  // current devices, remove the notification.
  for (const AudioDevice& device : hotplug_devices) {
    if (!IsDeviceInList(device, current_devices)) {
      // Remove notification and hotplug_input/output_devices_.
      auto* message_center = message_center::MessageCenter::Get();
      message_center->RemoveNotification(kAudioSelectionNotificationId,
                                         /*by_user=*/false);

      hotplug_input_devices_.clear();
      hotplug_output_devices_.clear();
      return;
    }
  }
}

}  // namespace ash
