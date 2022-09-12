// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_GRPC_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_GRPC_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/cast_core/runtime/browser/message_port_service.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_receiver/common/public/status.h"
#include "third_party/cast_core/public/src/proto/v2/core_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace chromecast {

class MessagePortHandler;

// This class defines a gRPC-based implementation of the MessagePortService
// interface, for use with Cast Core.
class MessagePortServiceGrpc : public MessagePortService {
 public:
  // |core_app_stub| must outlive |this|.
  MessagePortServiceGrpc(
      cast::v2::CoreMessagePortApplicationServiceStub* core_app_stub);
  ~MessagePortServiceGrpc() override;

  // MessagePortService implementation:
  cast_receiver::Status HandleMessage(cast::web::Message message) override;
  void ConnectToPortAsync(
      base::StringPiece port_name,
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
