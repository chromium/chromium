// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_GRPC_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_GRPC_H_

#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_receiver/browser/public/message_port_service.h"
#include "components/cast_receiver/common/public/status.h"
#include "third_party/cast_core/public/src/proto/v2/core_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace chromecast {

class MessagePortHandler;

// This class defines a gRPC-based implementation of the MessagePortService
// interface, for use with Cast Core.
class MessagePortServiceGrpc : public cast_receiver::MessagePortService {
 public:
  // |core_app_stub| must outlive |this|.
  MessagePortServiceGrpc(
      cast::v2::CoreMessagePortApplicationServiceStub* core_app_stub);
  ~MessagePortServiceGrpc() override;

  // Handles a message incoming over RPC. The message will be routed to the
  // appropriate destination based on its channel ID. Returns |true| in the case
  // that this message was successfully processed, and false in all other cases
  // including the case that there's no handler for the incoming channel ID.
  cast_receiver::Status HandleMessage(cast::web::Message message);

  // MessagePortService implementation:
  void ConnectToPortAsync(
      std::string_view port_name,
      std::unique_ptr<cast_api_bindings::MessagePort> port) override;
  uint32_t RegisterOutgoingPort(
      std::unique_ptr<cast_api_bindings::MessagePort> port) override;
  void RegisterIncomingPort(
      uint32_t channel_id,
      std::unique_ptr<cast_api_bindings::MessagePort> port) override;
  void Remove(uint32_t channel_id) override;

 private:
  std::unique_ptr<MessagePortHandler> MakeMessagePortHandler(
      uint32_t channel_id,
      std::unique_ptr<cast_api_bindings::MessagePort> port);

  // Callback invoked when AsyncConnect gets a gRPC result.
  void OnPortConnectionEstablished(
      uint32_t channel_id,
      cast::utils::GrpcStatusOr<cast::bindings::ConnectResponse> response_or);

  cast::v2::CoreMessagePortApplicationServiceStub* const core_app_stub_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  int next_outgoing_channel_id_{0};
  // NOTE: Keyed by channel_id of cast::web::MessageChannelDescriptor.
  base::flat_map<uint32_t, std::unique_ptr<MessagePortHandler>> ports_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MessagePortServiceGrpc> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_GRPC_H_
