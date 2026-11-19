// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_SENDER_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_SENDER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_target_device_info.h"

namespace sharing_message {
enum MessageType : int;
}  // namespace sharing_message

namespace syncer {
class LocalDeviceInfoProvider;
}  // namespace syncer

namespace sync_pb {
class UnencryptedSharingMessage;
}  // namespace sync_pb

enum class SharingChannelType;
class SharingChannelSender;
enum class SharingDevicePlatform;
enum class SharingSendMessageResult;

class SharingMessageSender {
 public:
  using ResponseCallback = base::OnceCallback<void(
      SharingSendMessageResult,
      std::unique_ptr<components_sharing_message::ResponseMessage>)>;

  SharingMessageSender(
      std::unique_ptr<SharingChannelSender> channel_sender,
      syncer::LocalDeviceInfoProvider* local_device_info_provider,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  SharingMessageSender(const SharingMessageSender&) = delete;
  SharingMessageSender& operator=(const SharingMessageSender&) = delete;
  virtual ~SharingMessageSender();

  virtual base::OnceClosure SendMessageToDevice(
      const SharingTargetDeviceInfo& device,
      base::TimeDelta response_timeout,
      components_sharing_message::SharingMessage message,
      ResponseCallback callback);

  virtual base::OnceClosure SendIosPushMessageToDevice(
      const SharingTargetDeviceInfo& device,
      sync_pb::UnencryptedSharingMessage message,
      ResponseCallback callback);

  virtual base::OnceClosure SendMessageToServerTarget(
      const components_sharing_message::ServerChannelConfiguration&
          server_channel,
      base::TimeDelta response_timeout,
      components_sharing_message::SharingMessage message,
      ResponseCallback callback);

  virtual void OnAckReceived(
      const std::string& message_id,
      std::unique_ptr<components_sharing_message::ResponseMessage> response);

  // Fails all pending messages with `SharingSendMessageResult::kInternalError`.
  virtual void FailPendingMessages();

  // Returns SharingChannelSender for testing.
  SharingChannelSender* GetChannelSenderForTesting() const;

 private:
  struct SentMessageMetadata {
    SentMessageMetadata(ResponseCallback callback,
                        base::TimeTicks timestamp,
                        sharing_message::MessageType type,
                        SharingDevicePlatform receiver_device_platform,
                        int trace_id,
                        base::TimeDelta receiver_pulse_interval);
    SentMessageMetadata(SentMessageMetadata&& other);
    SentMessageMetadata& operator=(SentMessageMetadata&& other);
    ~SentMessageMetadata();

    ResponseCallback callback;
    base::TimeTicks timestamp;
    sharing_message::MessageType type;
    SharingDevicePlatform receiver_device_platform;
    int trace_id;
    SharingChannelType channel_type = SharingChannelType::kUnknown;
    base::TimeDelta receiver_pulse_interval;
  };

  using SendTarget =
      std::variant<SharingTargetDeviceInfo,
                   components_sharing_message::ServerChannelConfiguration>;

  // Represents an outgoing message postponed until
  // `local_device_info_provider_` is initialized.
  struct PendingOutgoingMessage {
    PendingOutgoingMessage(std::string message_guid,
                           base::TimeDelta response_timeout,
                           components_sharing_message::SharingMessage message,
                           SendTarget target);
    PendingOutgoingMessage(const PendingOutgoingMessage&) = delete;
    PendingOutgoingMessage& operator=(const PendingOutgoingMessage&) = delete;
    PendingOutgoingMessage(PendingOutgoingMessage&& other) noexcept;
    PendingOutgoingMessage& operator=(PendingOutgoingMessage&& other) noexcept;
    ~PendingOutgoingMessage();

    std::string message_guid;
    base::TimeDelta response_timeout;
    components_sharing_message::SharingMessage message;
    SendTarget target;
  };

  void OnMessageSent(const std::string& message_guid,
                     sharing_message::MessageType message_type,
                     SharingSendMessageResult result,
                     std::optional<std::string> message_id,
                     SharingChannelType channel_type);

  void InvokeSendMessageCallback(
      const std::string& message_guid,
      SharingSendMessageResult result,
      std::unique_ptr<components_sharing_message::ResponseMessage> response);

  base::OnceClosure SendMessageToTarget(
      base::TimeDelta response_timeout,
      components_sharing_message::SharingMessage message,
      SendTarget target,
      ResponseCallback callback);

  // Populates sender device metadata and delegates sending `message` to
  // `channel_sender_`. Must only be called once `local_device_info_provider_`
  // is initialized.
  void DoSendMessageToTarget(const std::string& message_guid,
                             base::TimeDelta response_timeout,
                             components_sharing_message::SharingMessage message,
                             SendTarget target);

  // Invoked when `local_device_info_provider_` finishes initializing; sends all
  // queued `pending_outgoing_messages_`.
  void OnLocalDeviceInfoInitialized();

  static SentMessageMetadata CreateSentMessageMetadata(
      ResponseCallback callback,
      sharing_message::MessageType message_type,
      int trace_id,
      const SendTarget& target);

  std::unique_ptr<SharingChannelSender> channel_sender_;

  raw_ptr<syncer::LocalDeviceInfoProvider> local_device_info_provider_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Subscription for `local_device_info_provider_` initialization when
  // outgoing messages are waiting to be sent.
  base::CallbackListSubscription local_device_info_ready_subscription_;
  // Outgoing messages queued while waiting for `local_device_info_provider_`
  // to initialize.
  std::vector<PendingOutgoingMessage> pending_outgoing_messages_;

  // Map of random GUID to SentMessageMetadata.
  std::map<std::string, SentMessageMetadata> message_metadata_;
  // Map of FCM message_id to random GUID.
  std::map<std::string, std::string> message_guids_;
  // Map of FCM message_id to received ACK response messages.
  std::map<std::string,
           std::unique_ptr<components_sharing_message::ResponseMessage>>
      cached_ack_response_messages_;

  base::WeakPtrFactory<SharingMessageSender> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_SENDER_H_
