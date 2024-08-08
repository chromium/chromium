// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_fcm_handler.h"

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/proto/sharing_message_type.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_fcm_sender.h"
#include "components/sharing_message/sharing_handler_registry.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// The regex captures
// Group 1: type:timesmap
// Group 2: userId#
// Group 3: hashcode
const char kMessageIdRegexPattern[] = "(0:[0-9]+%)([0-9]+#)?([a-f0-9]+)";

// Returns message_id with userId stripped.
// FCM message_id is a persistent id in format of:
//     0:1416811810537717%0#e7a71353318775c7
//     ^          ^       ^          ^
// type :    timestamp % userId # hashcode
// As per go/persistent-id, userId# is optional, and should be stripped
// comparing persistent ids.
// Retrns |message_id| with userId stripped, or |message_id| if it is not
// confined to the format.
std::string GetStrippedMessageId(const std::string& message_id) {
  std::string stripped_message_id, type_timestamp, hashcode;
  static const base::NoDestructor<re2::RE2> kMessageIdRegex(
      kMessageIdRegexPattern);
  if (!re2::RE2::FullMatch(message_id, *kMessageIdRegex, &type_timestamp,
                           nullptr, &hashcode)) {
    return message_id;
  }
  return base::StrCat({type_timestamp, hashcode});
}

}  // namespace

SharingFCMHandler::SharingFCMHandler(
    gcm::GCMDriver* gcm_driver,
    syncer::DeviceInfoTracker* device_info_tracker,
    SharingFCMSender* sharing_fcm_sender,
    SharingHandlerRegistry* handler_registry)
    : gcm_driver_(gcm_driver),
      device_info_tracker_(device_info_tracker),
      sharing_fcm_sender_(sharing_fcm_sender),
      handler_registry_(handler_registry) {}

SharingFCMHandler::~SharingFCMHandler() {
  StopListening();
}

void SharingFCMHandler::StartListening() {
  if (!is_listening_) {
    gcm_driver_->AddAppHandler(kSharingFCMAppID, this);
    is_listening_ = true;
  }
}

void SharingFCMHandler::StopListening() {
  if (is_listening_) {
    gcm_driver_->RemoveAppHandler(kSharingFCMAppID);
    is_listening_ = false;
  }
}

void SharingFCMHandler::OnMessagesDeleted(const std::string& app_id) {
  // TODO: Handle message deleted from the server.
}

void SharingFCMHandler::ShutdownHandler() {
  is_listening_ = false;
}

void SharingFCMHandler::OnMessage(const std::string& app_id,
                                  const gcm::IncomingMessage& message) {
  TRACE_EVENT_BEGIN0("sharing", "SharingFCMHandler::OnMessage");

  components_sharing_message::SharingMessage sharing_message;
  if (!sharing_message.ParseFromString(message.raw_data)) {
    LOG(ERROR) << "Failed to parse incoming message with id : "
               << message.message_id;
    return;
  }
  components_sharing_message::SharingMessage::PayloadCase payload_case =
      sharing_message.payload_case();
  DCHECK(payload_case !=
         components_sharing_message::SharingMessage::PAYLOAD_NOT_SET)
      << "No payload set in SharingMessage received";

  sharing_message::MessageType message_type =
      SharingPayloadCaseToMessageType(payload_case);
  LogSharingMessageReceived(payload_case);

  SharingMessageHandler* handler =
      handler_registry_->GetSharingHandler(payload_case);
  if (!handler) {
    LOG(ERROR) << "No handler found for payload : " << payload_case;
  } else {
    SharingMessageHandler::DoneCallback done_callback = base::DoNothing();
    if (payload_case !=
        components_sharing_message::SharingMessage::kAckMessage) {
      std::string message_id = sharing_message.message_id();
      if (message_id.empty()) {
        message_id = GetStrippedMessageId(message.message_id);
      }
      done_callback = base::BindOnce(
          &SharingFCMHandler::SendAckMessage, weak_ptr_factory_.GetWeakPtr(),
          std::move(message_id), message_type, GetFCMChannel(sharing_message),
          GetServerChannel(sharing_message),
          GetSenderPlatform(sharing_message));
    }

    handler->OnMessage(std::move(sharing_message), std::move(done_callback));
  }

  TRACE_EVENT_END1("sharing", "SharingFCMHandler::OnMessage", "message_type",
                   SharingMessageTypeToString(message_type));
}

void SharingFCMHandler::OnSendAcknowledged(const std::string& app_id,
                                           const std::string& message_id) {
  NOTIMPLEMENTED();
}

void SharingFCMHandler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  NOTIMPLEMENTED();
}

void SharingFCMHandler::OnStoreReset() {
  // TODO: Handle GCM store reset.
}

std::optional<components_sharing_message::FCMChannelConfiguration>
SharingFCMHandler::GetFCMChannel(
    const components_sharing_message::SharingMessage& original_message) {
  if (!original_message.has_fcm_channel_configuration()) {
    return std::nullopt;
  }

  return original_message.fcm_channel_configuration();
}

std::optional<components_sharing_message::ServerChannelConfiguration>
SharingFCMHandler::GetServerChannel(
    const components_sharing_message::SharingMessage& original_message) {
  if (!original_message.has_server_channel_configuration()) {
    return std::nullopt;
  }

  return original_message.server_channel_configuration();
}

SharingDevicePlatform SharingFCMHandler::GetSenderPlatform(
    const components_sharing_message::SharingMessage& original_message) {
  const syncer::DeviceInfo* device_info =
      device_info_tracker_->GetDeviceInfo(original_message.sender_guid());
  if (!device_info) {
    return SharingDevicePlatform::kUnknown;
  }

  return GetDevicePlatform(*device_info);
}

void SharingFCMHandler::SendAckMessage(
    std::string original_message_id,
    sharing_message::MessageType original_message_type,
    std::optional<components_sharing_message::FCMChannelConfiguration>
        fcm_channel,
    std::optional<components_sharing_message::ServerChannelConfiguration>
        server_channel,
    SharingDevicePlatform sender_device_type,
    std::unique_ptr<components_sharing_message::ResponseMessage> response) {
  if (!fcm_channel && !server_channel) {
    LOG(ERROR) << "Unable to find ack channel configuration";
    return;
  }

  int trace_id = GenerateSharingTraceId();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "sharing", "Sharing.SendAckMessage", TRACE_ID_LOCAL(trace_id),
      "original_message_type",
      SharingMessageTypeToString(original_message_type));

  components_sharing_message::SharingMessage sharing_message;
  components_sharing_message::AckMessage* ack_message =
      sharing_message.mutable_ack_message();
  ack_message->set_original_message_id(original_message_id);
  if (response) {
    ack_message->set_allocated_response_message(response.release());
  }

  if (server_channel) {
    sharing_fcm_sender_->SendMessageToServerTarget(
        *server_channel, std::move(sharing_message),
        base::BindOnce(&SharingFCMHandler::OnAckMessageSent,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(original_message_id), original_message_type,
                       SharingDevicePlatform::kServer, trace_id));
    return;
  }

  sharing_fcm_sender_->SendMessageToFcmTarget(
      *fcm_channel, kSharingAckMessageTTL, std::move(sharing_message),
      base::BindOnce(&SharingFCMHandler::OnAckMessageSent,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(original_message_id), original_message_type,
                     sender_device_type, trace_id));
}

void SharingFCMHandler::OnAckMessageSent(
    std::string original_message_id,
    sharing_message::MessageType original_message_type,
    SharingDevicePlatform sender_device_type,
    int trace_id,
    SharingSendMessageResult result,
    std::optional<std::string> message_id,
    SharingChannelType channel_type) {
  if (result != SharingSendMessageResult::kSuccessful) {
    LOG(ERROR) << "Failed to send ack mesage for " << original_message_id;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END1("sharing", "Sharing.SendAckMessage",
                                  TRACE_ID_LOCAL(trace_id), "result",
                                  SharingSendMessageResultToString(result));
}
