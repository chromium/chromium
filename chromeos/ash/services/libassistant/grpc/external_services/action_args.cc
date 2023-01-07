// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/external_services/action_args.h"

#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/action_interface.pb.h"

namespace ash::libassistant {

namespace {

bool GetProtobufFromMap(const google::protobuf::RepeatedPtrField<
                            ::assistant::api::ProtobufMapEntry>& map,
                        const std::string& key,
                        const std::string& type,
                        std::string* protobuf_data) {
  for (const auto& entry : map) {
    if (!entry.has_value() || !entry.has_key() || key != entry.key())
      continue;
    const auto& pb = entry.value();
    if (type != pb.protobuf_type())
      continue;
    *protobuf_data = pb.protobuf_data();
    return true;
  }
  return false;
}

}  // namespace

ActionArgs::ActionArgs(const ::assistant::api::HandleActionRequest& request)
    : ActionArgs(request.conversation_id(),
                 request.user_id(),
                 request.event_id(),
                 request.interaction_id(),
                 request.args()) {}

ActionArgs::ActionArgs(const std::string& conversation_id,
                       const std::string& user_id,
                       const std::string& event_id,
                       int32_t interaction_id,
                       ::assistant::api::ClientOp::Args client_op_args)
    : conversation_id_(conversation_id),
      user_id_(user_id),
      event_id_(event_id),
      interaction_id_(interaction_id),
      client_op_args_(client_op_args) {}

ActionArgs::~ActionArgs() = default;

bool ActionArgs::GetProtobuf(const std::string& key,
                             const std::string& type,
                             std::string* protobuf_data) const {
  return GetProtobufFromMap(client_op_args_.arg(), key, type, protobuf_data);
}

std::string ActionArgs::GetConversationId() const {
  return conversation_id_;
}

std::string ActionArgs::GetUserId() const {
  return user_id_;
}

std::string ActionArgs::GetEventId() const {
  return event_id_;
}

int32_t ActionArgs::GetInteractionId() const {
  return interaction_id_;
}

assistant_client::ActionModule::Args* ActionArgs::Clone() const {
  return new ActionArgs(conversation_id_, user_id_, event_id_, interaction_id_,
                        client_op_args_);
}

}  // namespace ash::libassistant
