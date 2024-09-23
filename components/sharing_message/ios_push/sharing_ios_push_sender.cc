// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/ios_push/sharing_ios_push_sender.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "base/version.h"
#include "components/sharing_message/ios_push/ios_push_notification_util.h"
#include "components/sharing_message/proto/sharing_message_type.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_message_bridge.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/protocol/unencrypted_sharing_message.pb.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace sharing_message {

SharingIOSPushSender::SharingIOSPushSender(
    SharingMessageBridge* sharing_message_bridge,
    const syncer::DeviceInfoTracker* device_info_tracker,
    const syncer::LocalDeviceInfoProvider* local_device_info_provider,
    const syncer::SyncService* sync_service)
    : sharing_message_bridge_(sharing_message_bridge),
      device_info_tracker_(device_info_tracker),
      local_device_info_provider_(local_device_info_provider),
      sync_service_(sync_service) {
  CHECK(sync_service_);
}

SharingIOSPushSender::~SharingIOSPushSender() = default;

void SharingIOSPushSender::DoSendUnencryptedMessageToDevice(
    const SharingTargetDeviceInfo& device,
    sync_pb::UnencryptedSharingMessage message,
    SendMessageCallback callback) {
  TRACE_EVENT0("sharing", "SharingIOSPushSender::DoSendMessageToDevice");

  const syncer::DeviceInfo* target_device_info =
      device_info_tracker_->GetDeviceInfo(device.guid());

  sharing_message::MessageType message_type =
      SharingPayloadCaseToMessageType(message.payload_case());

  // Double check that device info is not null since the list of devices could
  // have been updated.
  if (!target_device_info) {
    std::move(callback).Run(
        SharingSendMessageResult::kDeviceNotFound,
        /*message_id=*/SharingMessageTypeToString(message_type),
        SharingChannelType::kIosPush);
    return;
  }

  const std::optional<syncer::DeviceInfo::SharingInfo>& sharing_info =
      target_device_info->sharing_info();
  if (!sharing_info.has_value() ||
      sharing_info.value().chime_representative_target_id.empty()) {
    std::move(callback).Run(
        SharingSendMessageResult::kDeviceNotFound,
        /*message_id=*/SharingMessageTypeToString(message_type),
        SharingChannelType::kIosPush);
    return;
  }

  if (message_type == sharing_message::SEND_TAB_TO_SELF_PUSH_NOTIFICATION &&
      !CanSendSendTabPushMessage(*target_device_info)) {
    std::move(callback).Run(
        SharingSendMessageResult::kInternalError,
        /*message_id=*/SharingMessageTypeToString(message_type),
        SharingChannelType::kIosPush);
    return;
  }

  std::string type_id = sharing_message::GetIosPushMessageTypeIdForChannel(
      message_type, local_device_info_provider_->GetChannel());

  auto specifics = std::make_unique<sync_pb::SharingMessageSpecifics>();
  sync_pb::SharingMessageSpecifics::ChannelConfiguration::
      ChimeChannelConfiguration* chime_configuration =
          specifics->mutable_channel_configuration()->mutable_chime();
  chime_configuration->set_channel_type(
      sync_pb::SharingMessageSpecifics::ChannelConfiguration::
          ChimeChannelConfiguration::APPLE_PUSH);
  chime_configuration->set_representative_target_id(
      sharing_info.value().chime_representative_target_id);
  chime_configuration->set_type_id(type_id);
  specifics->mutable_unencrypted_payload()->CopyFrom(message);

  sharing_message_bridge_->SendSharingMessage(
      std::move(specifics),
      base::BindOnce(&SharingIOSPushSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     /*message_id=*/SharingMessageTypeToString(message_type),
                     SharingChannelType::kIosPush));
}

bool SharingIOSPushSender::CanSendSendTabPushMessage(
    const syncer::DeviceInfo& target_device_info) {
  bool custom_passphrase_enabled =
      sync_service_->GetUserSettings()->IsUsingExplicitPassphrase();
  return target_device_info.send_tab_to_self_receiving_enabled() &&
         target_device_info.send_tab_to_self_receiving_type() ==
             sync_pb::SyncEnums::
                 SEND_TAB_RECEIVING_TYPE_CHROME_AND_PUSH_NOTIFICATION &&
         !custom_passphrase_enabled;
}

void SharingIOSPushSender::OnMessageSent(
    SendMessageCallback callback,
    const std::string& message_id,
    SharingChannelType channel_type,
    const sync_pb::SharingMessageCommitError& error) {
  TRACE_EVENT1("sharing", "SharingIOSPushSender::OnMessageSent", "error",
               error.error_code());

  SharingSendMessageResult send_message_result;
  switch (error.error_code()) {
    case sync_pb::SharingMessageCommitError::NONE:
      send_message_result = SharingSendMessageResult::kSuccessful;
      break;
    case sync_pb::SharingMessageCommitError::NOT_FOUND:
      send_message_result = SharingSendMessageResult::kDeviceNotFound;
      break;
    case sync_pb::SharingMessageCommitError::INVALID_ARGUMENT:
      send_message_result = SharingSendMessageResult::kPayloadTooLarge;
      break;
    case sync_pb::SharingMessageCommitError::INTERNAL:
    case sync_pb::SharingMessageCommitError::UNAVAILABLE:
    case sync_pb::SharingMessageCommitError::RESOURCE_EXHAUSTED:
    case sync_pb::SharingMessageCommitError::UNAUTHENTICATED:
    case sync_pb::SharingMessageCommitError::PERMISSION_DENIED:
    case sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF:
    case sync_pb::SharingMessageCommitError::
        DEPRECATED_SYNC_SERVER_OR_AUTH_ERROR:
    case sync_pb::SharingMessageCommitError::SYNC_SERVER_ERROR:
    case sync_pb::SharingMessageCommitError::SYNC_AUTH_ERROR:
      send_message_result = SharingSendMessageResult::kInternalError;
      break;
    case sync_pb::SharingMessageCommitError::SYNC_NETWORK_ERROR:
      send_message_result = SharingSendMessageResult::kNetworkError;
      break;
    case sync_pb::SharingMessageCommitError::SYNC_TIMEOUT:
      send_message_result = SharingSendMessageResult::kCommitTimeout;
      break;
  }

  std::move(callback).Run(send_message_result, message_id, channel_type);
}

}  // namespace sharing_message
