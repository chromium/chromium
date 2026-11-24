// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_PENDING_OUTGOING_MESSAGE_H_
#define COMPONENTS_SHARING_MESSAGE_PENDING_OUTGOING_MESSAGE_H_

#include <memory>
#include <string>
#include <variant>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_send_message_result.h"
#include "components/sharing_message/sharing_target_device_info.h"

namespace sharing_message {

// Represents an outgoing regular or ack message ready to be dispatched or
// postponed until readiness preconditions are met.
class PendingOutgoingMessage {
 public:
  using SendTarget =
      std::variant<SharingTargetDeviceInfo,
                   components_sharing_message::FCMChannelConfiguration,
                   components_sharing_message::ServerChannelConfiguration>;
  using SendAckCallback = base::OnceCallback<void(SharingSendMessageResult)>;

  // Creates a pending regular message tracked under `message_guid`.
  static std::unique_ptr<PendingOutgoingMessage> CreatePendingMessage(
      std::string message_guid,
      base::TimeDelta time_to_live,
      components_sharing_message::SharingMessage message,
      SendTarget target);

  // Creates a pending ack message that reports its send result via
  // `ack_callback`.
  static std::unique_ptr<PendingOutgoingMessage> CreatePendingAckMessage(
      components_sharing_message::SharingMessage message,
      SendTarget target,
      SendAckCallback ack_callback);

  // Consumes `pending_message` and returns its underlying `SharingMessage`
  // payload for sending.
  static components_sharing_message::SharingMessage TakeMessage(
      std::unique_ptr<PendingOutgoingMessage> pending_message);

  PendingOutgoingMessage(const PendingOutgoingMessage&) = delete;
  PendingOutgoingMessage& operator=(const PendingOutgoingMessage&) = delete;
  ~PendingOutgoingMessage();

  // Returns whether this message is an outgoing ack message.
  bool IsAckMessage() const;

  const std::string& message_guid() const { return message_guid_; }
  base::TimeDelta time_to_live() const { return time_to_live_; }
  const SendTarget& target() const { return target_; }

  // Moves out the ack completion callback. Must only be called for ack
  // messages, i.e. only if `IsAckMessage()` returns true.
  SendAckCallback TakeAckCallback();

 private:
  PendingOutgoingMessage(std::string message_guid,
                         base::TimeDelta time_to_live,
                         components_sharing_message::SharingMessage message,
                         SendTarget target,
                         SendAckCallback ack_callback);

  // Identifier of the outgoing message; empty for ack messages.
  std::string message_guid_;
  // FCM message expiration duration (`response_timeout` for regular messages,
  // `kSharingAckMessageTTL` for ack messages); unused for server targets.
  base::TimeDelta time_to_live_;
  components_sharing_message::SharingMessage message_;
  SendTarget target_;
  // Invoked with the send result for ack messages; null for regular messages.
  SendAckCallback ack_callback_;
};

}  // namespace sharing_message

#endif  // COMPONENTS_SHARING_MESSAGE_PENDING_OUTGOING_MESSAGE_H_
