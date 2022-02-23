// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_

#include <memory>
#include <string>

#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/services/libassistant/grpc/grpc_client_thread.h"
#include "chromeos/services/libassistant/grpc/grpc_state.h"
#include "chromeos/services/libassistant/grpc/grpc_util.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"

namespace chromeos {
namespace libassistant {

// Return gRPC method names.
template <typename Request>
std::string GetLibassistGrpcMethodName();

// Interface for all methods we as a client can invoke from Libassistant gRPC
// services. All client methods should be implemented here to send the requests
// to server. We only introduce methods that are currently in use.
class GrpcLibassistantClient {
 public:
  explicit GrpcLibassistantClient(std::shared_ptr<grpc::Channel> channel);
  GrpcLibassistantClient(const GrpcLibassistantClient&) = delete;
  GrpcLibassistantClient& operator=(const GrpcLibassistantClient&) = delete;
  ~GrpcLibassistantClient();

  // Calls an async client method. ResponseCallback will be invoked from
  // caller's sequence. The raw pointer will be handled by |RPCState| internally
  // and gets deleted upon completion of the RPC call.
  template <typename Request, typename Response>
  void CallServiceMethod(
      const Request& request,
      chromeos::libassistant::ResponseCallback<grpc::Status, Response> done,
      chromeos::libassistant::StateConfig state_config) {
    new chromeos::libassistant::RPCState<Response>(
        channel_, client_thread_.completion_queue(),
        GetLibassistGrpcMethodName<Request>(), request, std::move(done),
        /*callback_task_runner=*/base::SequencedTaskRunnerHandle::Get(),
        state_config);
  }

 private:
  // This channel will be shared between all stubs used to communicate with
  // multiple services. All channels are reference counted and will be freed
  // automatically.
  std::shared_ptr<grpc::Channel> channel_;

  // Thread running the completion queue.
  chromeos::libassistant::GrpcClientThread client_thread_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_
