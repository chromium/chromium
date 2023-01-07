// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_ACTION_ARGS_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_ACTION_ARGS_H_

#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/client_op.pb.h"

namespace assistant {
namespace api {
class HandleActionRequest;
}  // namespace api
}  // namespace assistant

namespace ash::libassistant {

class ActionArgs : public assistant_client::ActionModule::Args {
 public:
  explicit ActionArgs(const ::assistant::api::HandleActionRequest& request);
  ActionArgs(const std::string& conversation_id,
             const std::string& user_id,
             const std::string& event_id,
             int32_t interaction_id,
             ::assistant::api::ClientOp::Args client_op_args);
  ~ActionArgs() override;

  // assistant_client::ActionModule::Args:
  bool GetProtobuf(const std::string& key,
                   const std::string& type,
                   std::string* protobuf_data) const override;
  assistant_client::ActionModule::Args* Clone() const override;
  std::string GetConversationId() const override;
  std::string GetUserId() const override;
  std::string GetEventId() const override;
  int32_t GetInteractionId() const override;

 private:
  std::string conversation_id_;
  std::string user_id_;
  std::string event_id_;
  int32_t interaction_id_ = -1;
  ::assistant::api::ClientOp::Args client_op_args_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_ACTION_ARGS_H_
