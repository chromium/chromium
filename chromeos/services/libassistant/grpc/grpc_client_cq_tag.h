// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_CLIENT_CQ_TAG_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_CLIENT_CQ_TAG_H_

#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace chromeos {
namespace libassistant {

// Represents a pending asynchronous client call as a tag that can be
// stored in a |grpc::CompletionQueue|. Note that each |GrpcClientCQTag|
// will be responsible for deleting itself after an RPC is finished.
class GrpcClientCQTag {
 public:
  enum class State {
    kOk,
    kFailed,    // RPC failed.
    kShutdown,  // Client CQ has been shutdown.
  };

  GrpcClientCQTag() = default;
  GrpcClientCQTag(const GrpcClientCQTag&) = delete;
  GrpcClientCQTag& operator=(const GrpcClientCQTag&) = delete;
  virtual ~GrpcClientCQTag() = default;

  // OnCompleted is invoked when the RPC has finished.
  // Implementations of OnCompleted can delete *this.
  virtual void OnCompleted(State state) = 0;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_CLIENT_CQ_TAG_H_
