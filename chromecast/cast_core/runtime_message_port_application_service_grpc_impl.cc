// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime_message_port_application_service_grpc_impl.h"

#include "chromecast/cast_core/grpc_method.h"
#include "chromecast/cast_core/simple_async_grpc.h"

namespace chromecast {
namespace {

class PostMessage final : public SimpleAsyncGrpc<PostMessage,
                                                 cast::web::Message,
                                                 cast::web::MessagePortStatus> {
 public:
  PostMessage(
      cast::v2::RuntimeMessagePortApplicationService::AsyncService* service,
      RuntimeMessagePortApplicationServiceDelegate* delegate,
      ::grpc::ServerCompletionQueue* cq)
      : SimpleAsyncGrpc(cq), delegate_(delegate), service_(service) {
    service_->RequestPostMessage(&ctx_, &request_, &responder_, cq_, cq_,
                                 static_cast<GRPC*>(this));
  }

  cast::v2::RuntimeMessagePortApplicationService::AsyncService* service() {
    return service_;
  }

  RuntimeMessagePortApplicationServiceDelegate* delegate() { return delegate_; }

  void DoMethod() { delegate_->PostMessage(request_, &response_, this); }

 private:
  RuntimeMessagePortApplicationServiceDelegate* delegate_;
  cast::v2::RuntimeMessagePortApplicationService::AsyncService* service_;
};

}  // namespace

RuntimeMessagePortApplicationServiceDelegate::
    ~RuntimeMessagePortApplicationServiceDelegate() = default;

void StartRuntimeMessagePortApplicationServiceMethods(
    cast::v2::RuntimeMessagePortApplicationService::AsyncService* service,
    RuntimeMessagePortApplicationServiceDelegate* delegate,
    ::grpc::ServerCompletionQueue* cq) {
  new PostMessage(service, delegate, cq);
}

}  // namespace chromecast
