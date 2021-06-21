// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/grpc_libassistant_client.h"

#include <memory>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "chromeos/assistant/internal/libassistant_util.h"

namespace chromeos {
namespace libassistant {

namespace {

// Implements one async client method. ResponseCallback will run on completion
// queue thread |client_thread_|, so caller should bind its callback to its
// current sequence using BindToCurrentSequence() to make sure callback will
// always be invoked from the right sequence. The raw pointer will be handled by
// |RPCState| internally and gets deleted upon completion of the RPC call.
#define LIBAS_GRPC_CLIENT_METHOD(service, method)                            \
  void GrpcLibassistantClient::method(                                       \
      const chromeos::assistant::shared::method##Request& request,           \
      chromeos::libassistant::ResponseCallback<                              \
          grpc::Status, chromeos::assistant::shared::method##Response> done, \
      chromeos::libassistant::StateConfig state_config) {                    \
    new chromeos::libassistant::RPCState<                                    \
        chromeos::assistant::shared::method##Response>(                      \
        channel_, client_thread_.completion_queue(),                         \
        chromeos::assistant::GetLibassistGrpcMethodName(service, #method),   \
        request, std::move(done), state_config);                             \
  }

}  // namespace

GrpcLibassistantClient::GrpcLibassistantClient(
    std::shared_ptr<grpc::Channel> channel)
    : channel_(std::move(channel)), client_thread_("gRPCLibassistantClient") {
  DCHECK(channel_);
}

GrpcLibassistantClient::~GrpcLibassistantClient() = default;

LIBAS_GRPC_CLIENT_METHOD("CustomerRegistrationService", RegisterCustomer)

}  // namespace libassistant
}  // namespace chromeos
