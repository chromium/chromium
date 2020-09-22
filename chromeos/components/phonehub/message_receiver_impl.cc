// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/message_receiver_impl.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

#include <stdint.h>
#include <string>

#include "base/logging.h"

namespace chromeos {
namespace phonehub {
namespace {

std::string GetMessageTypeName(proto::MessageType message_type) {
  switch (message_type) {
    case proto::MessageType::PHONE_STATUS_SNAPSHOT:
      return "PHONE_STATUS_SNAPSHOT";
    case proto::MessageType::PHONE_STATUS_UPDATE:
      return "PHONE_STATUS_UPDATE";
    case proto::MessageType::UPDATE_NOTIFICATION_MODE_RESPONSE:
      return "UPDATE_NOTIFICATION_MODE_RESPONSE";
    case proto::MessageType::RING_DEVICE_RESPONSE:
      return "RING_DEVICE_RESPONSE";
    case proto::MessageType::UPDATE_BATTERY_MODE_RESPONSE:
      return "UPDATE_BATTERY_MODE_RESPONSE";
    case proto::MessageType::DISMISS_NOTIFICATION_RESPONSE:
      return "DISMISS_NOTIFICATION_RESPONSE";
    case proto::MessageType::NOTIFICATION_INLINE_REPLY_RESPONSE:
      return "NOTIFICATION_INLINE_REPLY_RESPONSE";
    case proto::MessageType::SHOW_NOTIFICATION_ACCESS_SETUP_RESPONSE:
      return "SHOW_NOTIFICATION_ACCESS_SETUP_RESPONSE";
    default:
      return "UNKOWN_MESSAGE";
  }
}

}  // namespace

MessageReceiverImpl::MessageReceiverImpl(ConnectionManager* connection_manager)
    : connection_manager_(connection_manager) {
  DCHECK(connection_manager_);

  connection_manager_->AddObserver(this);
}

MessageReceiverImpl::~MessageReceiverImpl() {
  connection_manager_->RemoveObserver(this);
}

void MessageReceiverImpl::OnMessageReceived(const std::string& payload) {
  // The first two bytes of |payload| is reserved for the header
  // proto::MessageType.
  uint16_t* ptr =
      reinterpret_cast<uint16_t*>(const_cast<char*>(payload.data()));
  proto::MessageType message_type = static_cast<proto::MessageType>(*ptr);

  PA_LOG(INFO) << "MessageReceiver received a "
               << GetMessageTypeName(message_type) << " message.";

  // Decode the proto message if the message is something we want to notify to
  // clients.
  if (message_type == proto::MessageType::PHONE_STATUS_SNAPSHOT) {
    proto::PhoneStatusSnapshot snapshot_proto;
    // Serialized proto is after the first two bytes of |payload|.
    if (!snapshot_proto.ParseFromString(payload.substr(2))) {
      PA_LOG(ERROR) << "OnMessageReceived() could not deserialize the "
                    << "PhoneStatusSnapshot proto message.";
      return;
    }
    NotifyPhoneStatusSnapshotReceived(snapshot_proto);
    return;
  }

  if (message_type == proto::MessageType::PHONE_STATUS_UPDATE) {
    proto::PhoneStatusUpdate update_proto;
    // Serialized proto is after the first two bytes of |payload|.
    if (!update_proto.ParseFromString(payload.substr(2))) {
      PA_LOG(ERROR) << "OnMessageReceived() could not deserialize the "
                    << "PhoneStatusUpdate proto message.";
      return;
    }
    NotifyPhoneStatusUpdateReceived(update_proto);
    return;
  }
}

}  // namespace phonehub
}  // namespace chromeos
