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
#include "chromecast/cast_core/runtime/browser/grpc/grpc_method.h"
#include "components/cast/message_port/message_port.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.grpc.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace chromecast {

class MessagePortHandler;

class MessagePortService {
 public:
  using CreatePairCallback = base::RepeatingCallback<void(
      std::unique_ptr<cast_api_bindings::MessagePort>*,
      std::unique_ptr<cast_api_bindings::MessagePort>*)>;

  // |grpc_cq| and |core_app_stub| must outlive |this|.
  MessagePortService(grpc::CompletionQueue* grpc_cq,
                     cast::v2::CoreApplicationService::Stub* core_app_stub);
  ~MessagePortService();

  // Handles a message incoming from the gRPC API.  The message will be routed
  // to the appropriate MessagePortHandler based on its channel ID.  |response|
  // is set to |OK| if MessagePortHandler reports success and |ERROR| otherwise,
  // including the case that there's no MessagePortHandler for the incoming
  // channel ID.
  void HandleMessage(const cast::web::Message& message,
                     cast::web::MessagePortStatus* response);

  // Connects |port| to the remote port with name |port_name|.
  bool ConnectToPort(base::StringPiece port_name,
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
  class AsyncConnect final : public GrpcCall {
   public:
    AsyncConnect(const cast::bindings::ConnectRequest& request,
                 cast::v2::CoreApplicationService::Stub* core_app_stub,
                 grpc::CompletionQueue* cq,
                 base::WeakPtr<MessagePortService> service);
    ~AsyncConnect() override;

    void StepGRPC(grpc::Status status) override;

   private:
    base::WeakPtr<MessagePortService> service_;
    cast::bindings::ConnectResponse response_;
    std::unique_ptr<
        grpc::ClientAsyncResponseReader<cast::bindings::ConnectResponse>>
        response_reader_;

    uint32_t channel_id_;
  };

  std::unique_ptr<MessagePortHandler> MakeMessagePortHandler(
      uint32_t channel_id,
      std::unique_ptr<cast_api_bindings::MessagePort> port);

  // Callback invoked when AsyncConnect gets a gRPC result.
  void OnConnectComplete(bool ok, uint32_t channel_id);

  CreatePairCallback create_pair_;

  grpc::CompletionQueue* grpc_cq_;
  cast::v2::CoreApplicationService::Stub* core_app_stub_;

  int next_outgoing_channel_id_{0};
  // NOTE: Keyed by channel_id of cast::web::MessageChannelDescriptor.
  base::flat_map<uint32_t, std::unique_ptr<MessagePortHandler>> ports_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MessagePortService> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_H_
