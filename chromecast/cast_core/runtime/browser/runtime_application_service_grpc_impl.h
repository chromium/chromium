// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_SERVICE_GRPC_IMPL_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_SERVICE_GRPC_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_application_service.grpc.pb.h"
#include "third_party/grpc/src/include/grpcpp/completion_queue.h"
#include "third_party/grpc/src/include/grpcpp/server_context.h"

namespace chromecast {

class GrpcMethod;

class RuntimeApplicationServiceDelegate {
 public:
  virtual ~RuntimeApplicationServiceDelegate() = 0;

  virtual void SetUrlRewriteRules(
      const cast::v2::SetUrlRewriteRulesRequest& request,
      cast::v2::SetUrlRewriteRulesResponse* response,
      GrpcMethod* callback) = 0;
};

void StartRuntimeApplicationServiceMethods(
    cast::v2::RuntimeApplicationService::AsyncService* service,
    base::WeakPtr<RuntimeApplicationServiceDelegate> delegate,
    ::grpc::ServerCompletionQueue* cq,
    bool* is_shutdown);

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_SERVICE_GRPC_IMPL_H_
