// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime_application_service_grpc_impl.h"

#include "chromecast/cast_core/grpc_method.h"
#include "chromecast/cast_core/simple_async_grpc.h"

namespace chromecast {
namespace {

class SetUrlRewriteRules final
    : public SimpleAsyncGrpc<SetUrlRewriteRules,
                             cast::v2::SetUrlRewriteRulesRequest,
                             cast::v2::SetUrlRewriteRulesResponse> {
 public:
  SetUrlRewriteRules(cast::v2::RuntimeApplicationService::AsyncService* service,
                     RuntimeApplicationServiceDelegate* delegate,
                     ::grpc::ServerCompletionQueue* cq)
      : SimpleAsyncGrpc(cq), delegate_(delegate), service_(service) {
    service_->RequestSetUrlRewriteRules(&ctx_, &request_, &responder_, cq_, cq_,
                                        static_cast<GRPC*>(this));
  }

  cast::v2::RuntimeApplicationService::AsyncService* service() {
    return service_;
  }

  RuntimeApplicationServiceDelegate* delegate() { return delegate_; }

  void DoMethod() { delegate_->SetUrlRewriteRules(request_, &response_, this); }

 private:
  RuntimeApplicationServiceDelegate* delegate_;
  cast::v2::RuntimeApplicationService::AsyncService* service_;
};

}  // namespace

RuntimeApplicationServiceDelegate::~RuntimeApplicationServiceDelegate() =
    default;

void StartRuntimeApplicationServiceMethods(
    cast::v2::RuntimeApplicationService::AsyncService* service,
    RuntimeApplicationServiceDelegate* delegate,
    ::grpc::ServerCompletionQueue* cq) {
  new SetUrlRewriteRules(service, delegate, cq);
}

}  // namespace chromecast
