// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_selection_notification_handler.h"

#include <cstring>
#include <optional>

#include "ash/strings/grit/ash_strings.h"
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
    const std::optional<std::string>& active_output_device_name) {
  // At least input or output has hotplug device.
  CHECK(!hotplug_input_devices.empty() || !hotplug_output_devices.empty());

  AudioDeviceList devices_to_activate;
  std::u16string title_message_id;
  std::u16string body_message_id;
  std::vector<message_center::ButtonInfo> buttons_info;

  NotificationTemplate notification_template = GetNotificationTemplate(
      hotplug_input_devices, hotplug_output_devices, active_input_device_name,
      active_output_device_name);

  // Use different notification titles and messages based on notification types.
  switch (notification_template.type) {
    case NotificationType::kSingleSourceWithInputOnly:
      title_message_id =
          l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_TITLE);
      body_message_id = l10n_util::GetStringFUTF16(
          IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_OR_OUTPUT_BODY,
          base::UTF8ToUTF16(hotplug_input_devices.front().display_name));
      devices_to_activate.push_back(hotplug_input_devices.front());
      buttons_info.emplace_back(
          l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_BUTTON_SWITCH));
      break;
    case NotificationType::kSingleSourceWithOutputOnly:
      title_message_id = l10n_util::GetStringUTF16(
          IDS_ASH_AUDIO_SELECTION_SWITCH_OUTPUT_TITLE);
      body_message_id = l10n_util::GetStringFUTF16(
          IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_OR_OUTPUT_BODY,
          base::UTF8ToUTF16(hotplug_output_devices.front().display_name));
      devices_to_activate.push_back(hotplug_output_devices.front());
      buttons_info.emplace_back(
          l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_BUTTON_SWITCH));
      break;
    case NotificationType::kSingleSourceWithInputAndOutput:
      title_message_id = l10n_util::GetStringUTF16(
          IDS_ASH_AUDIO_SELECTION_SWITCH_SOURCE_TITLE);
      body_message_id = l10n_util::GetStringFUTF16(
          IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_AND_OUTPUT_BODY,
          base::UTF8ToUTF16(
              ExtractDeviceSourceName(hotplug_output_devices.front())));
      devices_to_activate.push_back(hotplug_input_devices.front());
      devices_to_activate.push_back(hotplug_output_devices.front());
      buttons_info.emplace_back(
          l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_BUTTON_SWITCH));
      break;
    case NotificationType::kMultipleSources:
      title_message_id = l10n_util::GetStringUTF16(
          IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_TITLE);
      // TODO(zhangwenyu): Check with UX how to handle rare case where existing
      // devices' name is not available.
      body_message_id = l10n_util::GetStringFUTF16(
          IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_BODY,
          base::UTF8ToUTF16(active_input_device_name.has_value()
                                ? active_input_device_name.value()
                                : ""),
          base::UTF8ToUTF16(active_input_device_name.has_value()
                                ? active_output_device_name.value()
                                : ""));
      buttons_info.emplace_back(
          l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_BUTTON_SETTINGS));
      break;
  }

  message_center::RichNotificationData optional_fields;
  optional_fields.buttons = buttons_info;

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
          base::BindRepeating(
              &AudioSelectionNotificationHandler::HandleNotificationClicked,
              weak_ptr_factory_.GetWeakPtr(),
              /*is_settings_button=*/false, devices_to_activate))};
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification.id(),
                                     /*by_user=*/false);
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(notification));

  // TODO(zhangwenyu): Add metrics to record notification is displayed.
}

void AudioSelectionNotificationHandler::HandleNotificationClicked(
    bool is_settings_button,
    const AudioDeviceList& devices_to_activate,
    std::optional<int> button_index) {
  // TODO(zhangwenyu): Add metrics to record notification button clicked.
  // TODO(zhangwenyu): Activate the devices when switch button is clicked.
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

}  // namespace ash
