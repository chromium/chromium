// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_message_sender.h"

#include "base/containers/map_util.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/sharing_message/proto/sharing_message_type.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_fcm_sender.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync/protocol/unencrypted_sharing_message.pb.h"
#include "components/sync_device_info/device_name_util.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace {
bool MessageTypeExpectsAck(sharing_message::MessageType message_type) {
  switch (message_type) {
    case sharing_message::SEND_TAB_TO_SELF_PUSH_NOTIFICATION:
      return false;
    default:
      return true;
  }
}
}  // namespace

SharingMessageSender::SharingMessageSender(
    syncer::LocalDeviceInfoProvider* local_device_info_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : local_device_info_provider_(local_device_info_provider),
      task_runner_(task_runner) {}

SharingMessageSender::~SharingMessageSender() = default;

base::OnceClosure SharingMessageSender::SendMessageToDevice(
    const SharingTargetDeviceInfo& device,
    base::TimeDelta response_timeout,
    components_sharing_message::SharingMessage message,
    DelegateType delegate_type,
    ResponseCallback callback) {
  return SendMessageToTarget(delegate_type, response_timeout,
                             std::move(message), &device, std::move(callback));
}

base::OnceClosure SharingMessageSender::SendUnencryptedMessageToDevice(
    const SharingTargetDeviceInfo& device,
    sync_pb::UnencryptedSharingMessage message,
    DelegateType delegate_type,
    ResponseCallback callback) {
  sharing_message::MessageType message_type =
      SharingPayloadCaseToMessageType(message.payload_case());
  int trace_id = GenerateSharingTraceId();
  std::string message_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  TRACE_EVENT_BEGIN("sharing", "Sharing.SendMessage", perfetto::Track(trace_id),
                    "message_type", SharingMessageTypeToString(message_type));

  auto [it, inserted] = message_metadata_.insert_or_assign(
      message_guid,
      SentMessageMetadata(std::move(callback), base::TimeTicks::Now(),
                          message_type, device.platform(), trace_id,
                          device.pulse_interval()));
  DCHECK(inserted);

  SendMessageDelegate* delegate =
      base::FindPtrOrNull(send_delegates_, delegate_type);
  if (!delegate) {
    InvokeSendMessageCallback(message_guid,
                              SharingSendMessageResult::kInternalError,
                              /*response=*/nullptr);
    return base::NullCallback();
  }

  // TODO(crbug.com/40103693): Here we assume the caller gets the |device| from
  // GetDeviceCandidates, so LocalDeviceInfoProvider is ready. It's better to
  // queue up the message and wait until LocalDeviceInfoProvider is ready.
  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  if (!local_device_info) {
    InvokeSendMessageCallback(message_guid,
                              SharingSendMessageResult::kInternalError,
                              /*response=*/nullptr);
    return base::NullCallback();
  }

  message.set_sender_guid(local_device_info->guid());
  message.set_sender_device_name(
      syncer::GetDeviceDisplayNames(local_device_info).full_name);

  TRACE_EVENT_BEGIN("sharing", "Sharing.DoSendMessage",
                    perfetto::Track(trace_id));
  delegate->DoSendUnencryptedMessageToDevice(
      device, std::move(message),
      base::BindOnce(&SharingMessageSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), message_guid,
                     message_type));

  return base::BindOnce(&SharingMessageSender::InvokeSendMessageCallback,
                        weak_ptr_factory_.GetWeakPtr(), message_guid,
                        SharingSendMessageResult::kCancelled, nullptr);
}

base::OnceClosure SharingMessageSender::SendMessageToServerTarget(
    const components_sharing_message::ServerChannelConfiguration&
        server_channel,
    base::TimeDelta response_timeout,
    components_sharing_message::SharingMessage message,
    DelegateType delegate_type,
    ResponseCallback callback) {
  return SendMessageToTarget(delegate_type, response_timeout,
                             std::move(message), &server_channel,
                             std::move(callback));
}

void SharingMessageSender::OnMessageSent(
    const std::string& message_guid,
    sharing_message::MessageType message_type,
    SharingSendMessageResult result,
    std::optional<std::string> message_id,
    SharingChannelType channel_type) {
  auto metadata_iter = message_metadata_.find(message_guid);
  DCHECK(metadata_iter != message_metadata_.end());
  TRACE_EVENT_END("sharing", /* Sharing.DoSendMessage */
                  perfetto::Track(metadata_iter->second.trace_id), "result",
                  SharingSendMessageResultToString(result));
  metadata_iter->second.channel_type = channel_type;
  // For unsuccessful responses or for messages that don't expect an Ack
  // response, record the result here.
  if (result != SharingSendMessageResult::kSuccessful ||
      !MessageTypeExpectsAck(message_type)) {
    InvokeSendMessageCallback(message_guid, result,
                              /*response=*/nullptr);
    return;
  }

  // Got a new message id, store it for later.
  message_guids_.emplace(*message_id, message_guid);

  // Check if we got the ack while waiting for the FCM response.
  auto cached_iter = cached_ack_response_messages_.find(*message_id);
  if (cached_iter != cached_ack_response_messages_.end()) {
    OnAckReceived(*message_id, std::move(cached_iter->second));
    cached_ack_response_messages_.erase(cached_iter);
  }
}

void SharingMessageSender::OnAckReceived(
    const std::string& message_id,
    std::unique_ptr<components_sharing_message::ResponseMessage> response) {
  TRACE_EVENT0("sharing", "SharingMessageSender::OnAckReceived");
  auto guid_iter = message_guids_.find(message_id);
  if (guid_iter == message_guids_.end()) {
    // We don't have the guid yet, store the response until we receive it.
    cached_ack_response_messages_.emplace(message_id, std::move(response));
    return;
  }

  std::string message_guid = std::move(guid_iter->second);
  message_guids_.erase(guid_iter);

  auto metadata_iter = message_metadata_.find(message_guid);
  DCHECK(metadata_iter != message_metadata_.end());

  InvokeSendMessageCallback(message_guid, SharingSendMessageResult::kSuccessful,
                            std::move(response));

  message_metadata_.erase(metadata_iter);
}

void SharingMessageSender::RegisterSendDelegate(
    DelegateType type,
    std::unique_ptr<SendMessageDelegate> delegate) {
  auto result = send_delegates_.emplace(type, std::move(delegate));
  DCHECK(result.second) << "Delegate type already registered";
}

void SharingMessageSender::ClearPendingMessages() {
  for (const auto& [_, delegate] : send_delegates_) {
    delegate->ClearPendingMessages();
  }
}

SharingFCMSender* SharingMessageSender::GetFCMSenderForTesting() const {
  auto delegate_iter = send_delegates_.find(DelegateType::kFCM);
  DCHECK(delegate_iter != send_delegates_.end());
  DCHECK(delegate_iter->second);
  return static_cast<SharingFCMSender*>(delegate_iter->second.get());
}

void SharingMessageSender::InvokeSendMessageCallback(
    const std::string& message_guid,
    SharingSendMessageResult result,
    std::unique_ptr<components_sharing_message::ResponseMessage> response) {
  auto iter = message_metadata_.find(message_guid);
  if (iter == message_metadata_.end() || !iter->second.callback) {
    return;
  }

  SentMessageMetadata& metadata = iter->second;

  std::move(metadata.callback).Run(result, std::move(response));

  LogSendSharingMessageResult(metadata.type, metadata.receiver_device_platform,
                              metadata.channel_type,
                              metadata.receiver_pulse_interval, result);
  TRACE_EVENT_END(
      "sharing",
      /* SharingMessageSender.SendMessage */ perfetto::Track(metadata.trace_id),
      "result", SharingSendMessageResultToString(result));
}

base::OnceClosure SharingMessageSender::SendMessageToTarget(
    DelegateType delegate_type,
    base::TimeDelta response_timeout,
    components_sharing_message::SharingMessage message,
    std::variant<const SharingTargetDeviceInfo*,
                 const components_sharing_message::ServerChannelConfiguration*>
        target,
    ResponseCallback callback) {
  DCHECK(message.payload_case() !=
         components_sharing_message::SharingMessage::kAckMessage);

  SendMessageDelegate* delegate =
      base::FindPtrOrNull(send_delegates_, delegate_type);

  const int trace_id = GenerateSharingTraceId();
  const sharing_message::MessageType message_type =
      SharingPayloadCaseToMessageType(message.payload_case());
  const std::string message_guid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  TRACE_EVENT_BEGIN("sharing", "Sharing.SendMessage", perfetto::Track(trace_id),
                    "message_type", SharingMessageTypeToString(message_type));

  auto [it, inserted] = message_metadata_.insert_or_assign(
      message_guid, CreateSentMessageMetadata(std::move(callback), message_type,
                                              trace_id, target));
  DCHECK(inserted);

  if (!delegate) {
    InvokeSendMessageCallback(message_guid,
                              SharingSendMessageResult::kInternalError,
                              /*response=*/nullptr);
    return base::NullCallback();
  }

  // TODO(crbug.com/40103693): Here we assume the caller gets the |device| from
  // GetDeviceCandidates, so LocalDeviceInfoProvider is ready. It's better to
  // queue up the message and wait until LocalDeviceInfoProvider is ready.
  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  if (!local_device_info) {
    InvokeSendMessageCallback(message_guid,
                              SharingSendMessageResult::kInternalError,
                              /*response=*/nullptr);
    return base::NullCallback();
  }

  message.set_sender_guid(local_device_info->guid());
  message.set_sender_device_name(
      syncer::GetDeviceDisplayNames(local_device_info).full_name);

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SharingMessageSender::InvokeSendMessageCallback,
                     weak_ptr_factory_.GetWeakPtr(), message_guid,
                     SharingSendMessageResult::kAckTimeout,
                     /*response=*/nullptr),
      response_timeout);

  TRACE_EVENT_BEGIN("sharing", "Sharing.DoSendMessage",
                    perfetto::Track(trace_id));

  auto on_sent = base::BindOnce(&SharingMessageSender::OnMessageSent,
                                weak_ptr_factory_.GetWeakPtr(), message_guid,
                                message_type);

  std::visit(
      absl::Overload{
          [&](const SharingTargetDeviceInfo* device) {
            delegate->DoSendMessageToDevice(*device, response_timeout,
                                            std::move(message),
                                            std::move(on_sent));
          },
          [&](const components_sharing_message::ServerChannelConfiguration*
                  server_target) {
            delegate->DoSendMessageToServerTarget(
                *server_target, std::move(message), std::move(on_sent));
          }},
      target);

  return base::BindOnce(&SharingMessageSender::InvokeSendMessageCallback,
                        weak_ptr_factory_.GetWeakPtr(), message_guid,
                        SharingSendMessageResult::kCancelled, nullptr);
}

SharingMessageSender::SentMessageMetadata
SharingMessageSender::CreateSentMessageMetadata(
    ResponseCallback callback,
    sharing_message::MessageType message_type,
    int trace_id,
    std::variant<const SharingTargetDeviceInfo*,
                 const components_sharing_message::ServerChannelConfiguration*>
        target) {
  struct TargetData {
    SharingDevicePlatform receiver_device_platform;
    base::TimeDelta receiver_pulse_interval;
  };

  TargetData target_data = std::visit(
      absl::Overload{
          [](const SharingTargetDeviceInfo* device) -> TargetData {
            return {device->platform(), device->pulse_interval()};
          },
          [](const components_sharing_message::ServerChannelConfiguration*
                 server_channel) -> TargetData {
            return {SharingDevicePlatform::kServer, base::TimeDelta()};
          }},
      target);

  return SentMessageMetadata(std::move(callback), base::TimeTicks::Now(),
                             message_type, target_data.receiver_device_platform,
                             trace_id, target_data.receiver_pulse_interval);
}

SharingMessageSender::SentMessageMetadata::SentMessageMetadata(
    ResponseCallback callback,
    base::TimeTicks timestamp,
    sharing_message::MessageType type,
    SharingDevicePlatform receiver_device_platform,
    int trace_id,
    base::TimeDelta receiver_pulse_interval)
    : callback(std::move(callback)),
      timestamp(timestamp),
      type(type),
      receiver_device_platform(receiver_device_platform),
      trace_id(trace_id),
      receiver_pulse_interval(receiver_pulse_interval) {}

SharingMessageSender::SentMessageMetadata::SentMessageMetadata(
    SentMessageMetadata&& other) = default;

SharingMessageSender::SentMessageMetadata&
SharingMessageSender::SentMessageMetadata::operator=(
    SentMessageMetadata&& other) = default;

SharingMessageSender::SentMessageMetadata::~SentMessageMetadata() = default;
