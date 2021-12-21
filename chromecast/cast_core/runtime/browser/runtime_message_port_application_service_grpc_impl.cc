// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_message_port_application_service_grpc_impl.h"

#include "chromecast/cast_core/runtime/browser/grpc/grpc_method.h"
#include "chromecast/cast_core/runtime/browser/grpc/simple_async_grpc.h"

namespace chromecast {
namespace {

class PostMessage final : public SimpleAsyncGrpc<PostMessage,
                                                 cast::web::Message,
                                                 cast::web::MessagePortStatus> {
 public:
  PostMessage(
      cast::v2::RuntimeMessagePortApplicationService::AsyncService* service,
      base::WeakPtr<RuntimeMessagePortApplicationServiceDelegate> delegate,
      ::grpc::ServerCompletionQueue* cq,
      bool* is_shutdown)
      : SimpleAsyncGrpc(cq),
        is_shutdown_(is_shutdown),
        delegate_(delegate),
        service_(service) {
    DCHECK(!*is_shutdown_);
    service_->RequestPostMessage(&ctx_, &request_, &responder_, cq_, cq_,
                                 static_cast<GRPC*>(this));
  }

  cast::v2::RuntimeMessagePortApplicationService::AsyncService* service() {
    return service_;
  }

  base::WeakPtr<RuntimeMessagePortApplicationServiceDelegate> delegate() {
    return delegate_;
  }

  bool* is_shutdown() { return is_shutdown_; }

  void DoMethod() { delegate_->PostMessage(request_, &response_, this); }

 private:
  bool* is_shutdown_;
  base::WeakPtr<RuntimeMessagePortApplicationServiceDelegate> delegate_;
  cast::v2::RuntimeMessagePortApplicationService::AsyncService* service_;
};

}  // namespace

RuntimeMessagePortApplicationServiceDelegate::
    ~RuntimeMessagePortApplicationServiceDelegate() = default;

void StartRuntimeMessagePortApplicationServiceMethods(
    cast::v2::RuntimeMessagePortApplicationService::AsyncService* service,
    base::WeakPtr<RuntimeMessagePortApplicationServiceDelegate> delegate,
    ::grpc::ServerCompletionQueue* cq,
    bool* is_shutdown) {
  new PostMessage(service, delegate, cq, is_shutdown);
}

}  // namespace chromecast
