// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_CALL_OPTIONS_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_CALL_OPTIONS_H_

#include <grpcpp/grpcpp.h>

#include "base/time/time.h"

namespace grpc {
class ClientContext;
}

namespace cast {
namespace utils {

// Various options that control gRPC ClientContext behavior.
class GrpcCallOptions {
 public:
  GrpcCallOptions();
  ~GrpcCallOptions();

  // Sets the client call deadline.
  void SetDeadline(base::TimeDelta deadline);

  // Applies the options to a give grpc client |context|.
  void ApplyOptionsToContext(grpc::ClientContext* context) const;

  // Converts a TimeDelta to gRPC's gpr_timespec.
  static gpr_timespec ToGprTimespec(const base::TimeDelta& delta);

 private:
  // gRPC read request deadline.
  base::TimeDelta deadline_;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_CALL_OPTIONS_H_
