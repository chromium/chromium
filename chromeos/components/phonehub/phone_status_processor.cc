// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/phone_status_processor.h"

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/components/phonehub/do_not_disturb_controller.h"
#include "chromeos/components/phonehub/find_my_device_controller.h"
#include "chromeos/components/phonehub/message_receiver.h"
#include "chromeos/components/phonehub/mutable_phone_model.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/notification_access_manager.h"
#include "chromeos/components/phonehub/notification_manager.h"
#include "ui/gfx/image/image.h"

#include <algorithm>
#include <string>

namespace chromeos {
namespace phonehub {
namespace {
using multidevice_setup::MultiDeviceSetupClient;

gfx::Image CreateImageFromSerializedIcon(const std::string& bytes) {
  return gfx::Image::CreateFrom1xPNGBytes(
      reinterpret_cast<const unsigned char*>(bytes.c_str()), bytes.size());
}

Notification::Importance GetNotificationImportanceFromProto(
    proto::NotificationImportance importance) {
  switch (importance) {
    case proto::NotificationImportance::UNSPECIFIED:
      return Notification::Importance::kUnspecified;
    case proto::NotificationImportance::NONE:
      return Notification::Importance::kNone;
    case proto::NotificationImportance::MIN:
      return Notification::Importance::kMin;
    case proto::NotificationImportance::LOW:
      return Notification::Importance::kLow;
    case proto::NotificationImportance::DEFAULT:
      return Notification::Importance::kDefault;
    case proto::NotificationImportance::HIGH:
      return Notification::Importance::kHigh;
    default:
      return Notification::Importance::kUnspecified;
  }
}

PhoneStatusModel::MobileStatus GetMobileStatusFromProto(
    proto::MobileConnectionState mobile_status) {
  switch (mobile_status) {
    case proto::MobileConnectionState::NO_SIM:
      return PhoneStatusModel::MobileStatus::kNoSim;
    case proto::MobileConnectionState::SIM_BUT_NO_RECEPTION:
      return PhoneStatusModel::MobileStatus::kSimButNoReception;
    case proto::MobileConnectionState::SIM_WITH_RECEPTION:
      return PhoneStatusModel::MobileStatus::kSimWithReception;
    default:
      return PhoneStatusModel::MobileStatus::kNoSim;
  }
}

PhoneStatusModel::SignalStrength GetSignalStrengthFromProto(
    proto::SignalStrength signal_strength) {
  switch (signal_strength) {
    case proto::SignalStrength::ZERO_BARS:
      return PhoneStatusModel::SignalStrength::kZeroBars;
    case proto::SignalStrength::ONE_BAR:
      return PhoneStatusModel::SignalStrength::kOneBar;
    case proto::SignalStrength::TWO_BARS:
      return PhoneStatusModel::SignalStrength::kTwoBars;
    case proto::SignalStrength::THREE_BARS:
      return PhoneStatusModel::SignalStrength::kThreeBars;
    case proto::SignalStrength::FOUR_BARS:
      return PhoneStatusModel::SignalStrength::kFourBars;
    default:
      return PhoneStatusModel::SignalStrength::kZeroBars;
  }
}

PhoneStatusModel::ChargingState GetChargingStateFromProto(
    proto::ChargingState charging_state) {
  switch (charging_state) {
    case proto::ChargingState::NOT_CHARGING:
      return PhoneStatusModel::ChargingState::kNotCharging;
    case proto::ChargingState::CHARGING_AC:
    case proto::ChargingState::CHARGING_WIRELESS:
      return PhoneStatusModel::ChargingState::kChargingAc;
    case proto::ChargingState::CHARGING_USB:
      return PhoneStatusModel::ChargingState::kChargingUsb;
    default:
      return PhoneStatusModel::ChargingState::kNotCharging;
  }
}

PhoneStatusModel::BatterySaverState GetBatterySaverStateFromProto(
    proto::BatteryMode battery_mode) {
  switch (battery_mode) {
    case proto::BatteryMode::BATTERY_SAVER_OFF:
      return PhoneStatusModel::BatterySaverState::kOff;
    case proto::BatteryMode::BATTERY_SAVER_ON:
      return PhoneStatusModel::BatterySaverState::kOn;
    default:
      return PhoneStatusModel::BatterySaverState::kOff;
  }
}

base::Optional<Notification> ProcessNotificationProto(
    const proto::Notification& proto) {
  // Only process notifications that are messaging apps with inline-replies.
  auto actions_it = std::find_if(
      proto.actions().begin(), proto.actions().end(), [](const auto& action) {
        return action.type() == proto::Action_InputType::Action_InputType_TEXT;
      });

  if (actions_it == proto.actions().end())
    return base::nullopt;

  base::Optional<base::string16> title = base::nullopt;
  if (!proto.title().empty())
    title = base::UTF8ToUTF16(proto.title());

  base::Optional<base::string16> text_content = base::nullopt;
  if (!proto.text_content().empty())
    text_content = base::UTF8ToUTF16(proto.text_content());

  base::Optional<gfx::Image> shared_image = base::nullopt;
  if (!proto.shared_image().empty())
    shared_image = CreateImageFromSerializedIcon(proto.shared_image());

  base::Optional<gfx::Image> contact_image = base::nullopt;
  if (!proto.contact_image().empty())
    contact_image = CreateImageFromSerializedIcon(proto.contact_image());

  return Notification(
      proto.id(),
      Notification::AppMetadata(
          base::UTF8ToUTF16(proto.origin_app().visible_name()),
          proto.origin_app().package_name(),
          CreateImageFromSerializedIcon(proto.origin_app().icon())),
      base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMilliseconds(proto.epoch_time_millis())),
      GetNotificationImportanceFromProto(proto.importance()), actions_it->id(),
      title, text_content, shared_image, contact_image);
}

PhoneStatusModel CreatePhoneStatusModel(const proto::PhoneProperties& proto) {
  return PhoneStatusModel(
      GetMobileStatusFromProto(proto.connection_state()),
      PhoneStatusModel::MobileConnectionMetadata{
          GetSignalStrengthFromProto(proto.signal_strength()),
          base::UTF8ToUTF16(proto.mobile_provider())},
      GetChargingStateFromProto(proto.charging_state()),
      GetBatterySaverStateFromProto(proto.battery_mode()),
      proto.battery_percentage());
}

}  // namespace

PhoneStatusProcessor::PhoneStatusProcessor(
    DoNotDisturbController* do_not_disturb_controller,
    FeatureStatusProvider* feature_status_provider,
    MessageReceiver* message_receiver,
    FindMyDeviceController* find_my_device_controller,
    NotificationAccessManager* notification_access_manager,
    NotificationManager* notification_manager,
    MultiDeviceSetupClient* multidevice_setup_client,
    MutablePhoneModel* phone_model)
    : do_not_disturb_controller_(do_not_disturb_controller),
      feature_status_provider_(feature_status_provider),
      message_receiver_(message_receiver),
      find_my_device_controller_(find_my_device_controller),
      notification_access_manager_(notification_access_manager),
      notification_manager_(notification_manager),
      multidevice_setup_client_(multidevice_setup_client),
      phone_model_(phone_model) {
  DCHECK(do_not_disturb_controller_);
  DCHECK(feature_status_provider_);
  DCHECK(message_receiver_);
  DCHECK(find_my_device_controller_);
  DCHECK(notification_access_manager_);
  DCHECK(notification_manager_);
  DCHECK(multidevice_setup_client_);
  DCHECK(phone_model_);

  message_receiver_->AddObserver(this);
  feature_status_provider_->AddObserver(this);

  MaybeSetPhoneModelName(multidevice_setup_client_->GetHostStatus().second);
}

PhoneStatusProcessor::~PhoneStatusProcessor() {
  message_receiver_->RemoveObserver(this);
  feature_status_provider_->RemoveObserver(this);
}

void PhoneStatusProcessor::SetReceivedNotifications(
    const RepeatedPtrField<proto::Notification>& notification_protos) {
  base::flat_set<Notification> notifications;

  for (const auto& proto : notification_protos) {
    base::Optional<Notification> notif = ProcessNotificationProto(proto);
    if (notif.has_value())
      notifications.emplace(*notif);
  }
  notification_manager_->SetNotificationsInternal(notifications);
}

void PhoneStatusProcessor::SetReceivedPhoneStatusModelStates(
    const proto::PhoneProperties& phone_properties) {
  phone_model_->SetPhoneStatusModel(CreatePhoneStatusModel(phone_properties));

  do_not_disturb_controller_->SetDoNotDisturbStateInternal(
      phone_properties.notification_mode() ==
      proto::NotificationMode::DO_NOT_DISTURB_ON);

  notification_access_manager_->SetHasAccessBeenGrantedInternal(
      phone_properties.notification_access_state() ==
      proto::NotificationAccessState::ACCESS_GRANTED);

  find_my_device_controller_->SetIsPhoneRingingInternal(
      phone_properties.ring_status() == proto::FindMyDeviceRingStatus::RINGING);
}

void PhoneStatusProcessor::MaybeSetPhoneModelName(
    const base::Optional<multidevice::RemoteDeviceRef>& remote_device) {
  if (!remote_device.has_value()) {
    phone_model_->SetPhoneName(base::nullopt);
    return;
  }

  phone_model_->SetPhoneName(base::UTF8ToUTF16(remote_device->name()));
}

void PhoneStatusProcessor::OnFeatureStatusChanged() {
  // Reset phone model instance when but still keep the phone's name.
  if (feature_status_provider_->GetStatus() !=
      FeatureStatus::kEnabledAndConnected) {
    phone_model_->SetPhoneStatusModel(base::nullopt);
    notification_manager_->ClearNotificationsInternal();
  }
}

void PhoneStatusProcessor::OnPhoneStatusSnapshotReceived(
    proto::PhoneStatusSnapshot phone_status_snapshot) {
  SetReceivedNotifications(phone_status_snapshot.notifications());
  SetReceivedPhoneStatusModelStates(phone_status_snapshot.properties());
}

void PhoneStatusProcessor::OnPhoneStatusUpdateReceived(
    proto::PhoneStatusUpdate phone_status_update) {
  SetReceivedNotifications(phone_status_update.updated_notifications());
  SetReceivedPhoneStatusModelStates(phone_status_update.properties());

  base::flat_set<int64_t> removed_notification_ids;
  for (auto& id : phone_status_update.removed_notification_ids()) {
    removed_notification_ids.emplace(id);
  }
  notification_manager_->RemoveNotificationsInternal(removed_notification_ids);
}

void PhoneStatusProcessor::OnHostStatusChanged(
    const MultiDeviceSetupClient::HostStatusWithDevice&
        host_device_with_status) {
  MaybeSetPhoneModelName(host_device_with_status.second);
}

}  // namespace phonehub
}  // namespace chromeos
