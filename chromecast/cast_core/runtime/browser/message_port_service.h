// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast/message_port/message_port.h"
#include "third_party/cast_core/public/src/proto/v2/core_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace chromecast {

class MessagePortHandler;

class MessagePortService {
 public:
  using CreatePairCallback = base::RepeatingCallback<void(
      std::unique_ptr<cast_api_bindings::MessagePort>*,
      std::unique_ptr<cast_api_bindings::MessagePort>*)>;

  // |core_app_stub| must outlive |this|.
  MessagePortService(
      cast::v2::CoreMessagePortApplicationServiceStub* core_app_stub);
  ~MessagePortService();

  // Handles a message incoming from the gRPC API.  The message will be routed
  // to the appropriate MessagePortHandler based on its channel ID. |response|
  // is set to |OK| if MessagePortHandler reports success and |ERROR|
  // otherwise, including the case that there's no MessagePortHandler for the
  // incoming channel ID.
  cast::utils::GrpcStatusOr<cast::web::MessagePortStatus> HandleMessage(
      cast::web::Message message);

  // Connects |port| to the remote port with name |port_name|. Calls the
  // callback with grpc::Status code.
  void ConnectToPort(base::StringPiece port_name,
                     std::unique_ptr<cast_api_bindings::MessagePort> port);

  // Registers a port opened locally via a port transfer.  This allocates a
  // |channel_id| for the port in order to send it over gRPC.
  uint32_t RegisterOutgoingPort(
      std::unique_ptr<cast_api_bindings::MessagePort> port);

  // Registers a port opened by the peer via a port transfer.  |channel_id| is
  // provided by the peer.
  void RegisterIncomingPort(
      uint32_t channel_id,
      std::unique_ptr<cast_api_bindings::MessagePort> port);

  // Removes the MessagePortHandler for |channel_id|.  Note that this will
  // destroy it.
  void Remove(uint32_t channel_id);

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
  base::WeakPtrFactory<MessagePortService> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_H_
