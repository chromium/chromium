// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_

#include <memory>
#include <string>

#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_client_thread.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_state.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"

namespace ash::libassistant {

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
  void CallServiceMethod(const Request& request,
                         ResponseCallback<grpc::Status, Response> done,
                         StateConfig state_config) {
    new RPCState<Response>(
        channel_, client_thread_.completion_queue(),
        GetLibassistGrpcMethodName<Request>(), request, std::move(done),
        /*callback_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
        state_config);
  }

 private:
  // This channel will be shared between all stubs used to communicate with
  // multiple services. All channels are reference counted and will be freed
  // automatically.
  std::shared_ptr<grpc::Channel> channel_;

  // Thread running the completion queue.
  GrpcClientThread client_thread_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_
