// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_message_sender.h"

#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sharing_message/proto/sharing_message_type.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_fcm_sender.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync/protocol/unencrypted_sharing_message.pb.h"
#include "components/sync_device_info/local_device_info_provider.h"

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
  DCHECK(message.payload_case() !=
         components_sharing_message::SharingMessage::kAckMessage);

  sharing_message::MessageType message_type =
      SharingPayloadCaseToMessageType(message.payload_case());
  int trace_id = GenerateSharingTraceId();
  std::string message_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("sharing", "Sharing.SendMessage",
                                    TRACE_ID_LOCAL(trace_id), "message_type",
                                    SharingMessageTypeToString(message_type));

  auto [it, inserted] = message_metadata_.insert_or_assign(
      message_guid, SentMessageMetadata(
                        std::move(callback), base::TimeTicks::Now(),
                        message_type, device.platform(), trace_id,
                        SharingChannelType::kUnknown, device.pulse_interval()));
  DCHECK(inserted);

  SendMessageDelegate* delegate = MaybeGetSendMessageDelegate(
      device, message_type, trace_id, message_guid, delegate_type);
  if (!delegate) {
    InvokeSendMessageCallback(message_guid,
                              SharingSendMessageResult::kInternalError,
                              /*response=*/nullptr);
    return base::NullCallback();
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SharingMessageSender::InvokeSendMessageCallback,
                     weak_ptr_factory_.GetWeakPtr(), message_guid,
                     SharingSendMessageResult::kAckTimeout,
                     /*response=*/nullptr),
      response_timeout);

  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();

  // Guaranteed by MaybeGetSendMessageDelegate().
  CHECK(local_device_info);

  message.set_sender_guid(local_device_info->guid());
  message.set_sender_device_name(
      send_tab_to_self::GetSharingDeviceNames(local_device_info).full_name);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("sharing", "Sharing.DoSendMessage",
                                    TRACE_ID_LOCAL(trace_id));

  delegate->DoSendMessageToDevice(
      device, response_timeout, std::move(message),
      base::BindOnce(&SharingMessageSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), message_guid,
                     message_type));

  return base::BindOnce(&SharingMessageSender::InvokeSendMessageCallback,
                        weak_ptr_factory_.GetWeakPtr(), message_guid,
                        SharingSendMessageResult::kCancelled, nullptr);
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

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("sharing", "Sharing.SendMessage",
                                    TRACE_ID_LOCAL(trace_id), "message_type",
                                    SharingMessageTypeToString(message_type));

  auto [it, inserted] = message_metadata_.insert_or_assign(
      message_guid, SentMessageMetadata(
                        std::move(callback), base::TimeTicks::Now(),
                        message_type, device.platform(), trace_id,
                        SharingChannelType::kUnknown, device.pulse_interval()));
  DCHECK(inserted);

  SendMessageDelegate* delegate = MaybeGetSendMessageDelegate(
      device, message_type, trace_id, message_guid, delegate_type);
  if (!delegate) {
    InvokeSendMessageCallback(message_guid,
                              SharingSendMessageResult::kInternalError,
                              /*response=*/nullptr);
    return base::NullCallback();
  }

  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();

  // Guaranteed by MaybeGetSendMessageDelegate().
  CHECK(local_device_info);

  message.set_sender_guid(local_device_info->guid());
  message.set_sender_device_name(
      send_tab_to_self::GetSharingDeviceNames(local_device_info).full_name);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("sharing", "Sharing.DoSendMessage",
                                    TRACE_ID_LOCAL(trace_id));
  delegate->DoSendUnencryptedMessageToDevice(
      device, std::move(message),
      base::BindOnce(&SharingMessageSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), message_guid,
                     message_type));

  return base::BindOnce(&SharingMessageSender::InvokeSendMessageCallback,
                        weak_ptr_factory_.GetWeakPtr(), message_guid,
                        SharingSendMessageResult::kCancelled, nullptr);
}

SharingMessageSender::SendMessageDelegate*
SharingMessageSender::MaybeGetSendMessageDelegate(
    const SharingTargetDeviceInfo& device,
    sharing_message::MessageType message_type,
    int trace_id,
    const std::string& message_guid,
    DelegateType delegate_type) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("sharing", "Sharing.SendMessage",
                                    TRACE_ID_LOCAL(trace_id), "message_type",
                                    SharingMessageTypeToString(message_type));
  auto delegate_iter = send_delegates_.find(delegate_type);
  if (delegate_iter == send_delegates_.end()) {
    return nullptr;
  }
  SendMessageDelegate* delegate = delegate_iter->second.get();
  DCHECK(delegate);

  // TODO(crbug.com/40103693): Here we assume the caller gets the |device| from
  // GetDeviceCandidates, so LocalDeviceInfoProvider is ready. It's better to
  // queue up the message and wait until LocalDeviceInfoProvider is ready.
  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  if (!local_device_info) {
    return nullptr;
  }

  return delegate;
}

void SharingMessageSender::OnMessageSent(
    const std::string& message_guid,
    sharing_message::MessageType message_type,
    SharingSendMessageResult result,
    std::optional<std::string> message_id,
    SharingChannelType channel_type) {
  auto metadata_iter = message_metadata_.find(message_guid);
  DCHECK(metadata_iter != message_metadata_.end());
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "sharing", "Sharing.DoSendMessage",
      TRACE_ID_LOCAL(metadata_iter->second.trace_id), "result",
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
  TRACE_EVENT_NESTABLE_ASYNC_END1("sharing", "SharingMessageSender.SendMessage",
                                  TRACE_ID_LOCAL(metadata.trace_id), "result",
                                  SharingSendMessageResultToString(result));
}

SharingMessageSender::SentMessageMetadata::SentMessageMetadata(
    ResponseCallback callback,
    base::TimeTicks timestamp,
    sharing_message::MessageType type,
    SharingDevicePlatform receiver_device_platform,
    int trace_id,
    SharingChannelType channel_type,
    base::TimeDelta receiver_pulse_interval)
    : callback(std::move(callback)),
      timestamp(timestamp),
      type(type),
      receiver_device_platform(receiver_device_platform),
      trace_id(trace_id),
      channel_type(channel_type),
      receiver_pulse_interval(receiver_pulse_interval) {}

SharingMessageSender::SentMessageMetadata::SentMessageMetadata(
    SentMessageMetadata&& other) = default;

SharingMessageSender::SentMessageMetadata&
SharingMessageSender::SentMessageMetadata::operator=(
    SentMessageMetadata&& other) = default;

SharingMessageSender::SentMessageMetadata::~SentMessageMetadata() = default;
