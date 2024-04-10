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

constexpr char kAudioSelectionNotificationId[] = "audio_selection_notification";
constexpr char kAudioSelectionNotifierId[] =
    "ash.audio_selection_notification_handler";

}  // namespace

AudioSelectionNotificationHandler::AudioSelectionNotificationHandler() =
    default;
AudioSelectionNotificationHandler::~AudioSelectionNotificationHandler() =
    default;

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

  if (!hotplug_input_devices.empty()) {
    devices_to_activate.push_back(hotplug_input_devices.front());
    title_message_id =
        l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_TITLE);
    body_message_id = l10n_util::GetStringFUTF16(
        IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_OR_OUTPUT_BODY,
        base::UTF8ToUTF16(hotplug_input_devices.front().display_name));
  } else {
    devices_to_activate.push_back(hotplug_output_devices.front());
    title_message_id =
        l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SWITCH_OUTPUT_TITLE);
    body_message_id = l10n_util::GetStringFUTF16(
        IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_OR_OUTPUT_BODY,
        base::UTF8ToUTF16(hotplug_output_devices.front().display_name));
  }

  std::vector<message_center::ButtonInfo> buttons_info;
  buttons_info.emplace_back(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_BUTTON_SWITCH));
  message_center::RichNotificationData optional_fields;
  optional_fields.buttons = buttons_info;

  // TODO(zhangwenyu): Handle other notification types.

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

}  // namespace ash
