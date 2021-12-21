// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_service_grpc_impl.h"

#include "chromecast/cast_core/runtime/browser/grpc/grpc_method.h"
#include "chromecast/cast_core/runtime/browser/grpc/simple_async_grpc.h"

namespace chromecast {
namespace {

class SetUrlRewriteRules final
    : public SimpleAsyncGrpc<SetUrlRewriteRules,
                             cast::v2::SetUrlRewriteRulesRequest,
                             cast::v2::SetUrlRewriteRulesResponse> {
 public:
  SetUrlRewriteRules(cast::v2::RuntimeApplicationService::AsyncService* service,
                     base::WeakPtr<RuntimeApplicationServiceDelegate> delegate,
                     ::grpc::ServerCompletionQueue* cq,
                     bool* is_shutdown)
      : SimpleAsyncGrpc(cq),
        is_shutdown_(is_shutdown),
        delegate_(delegate),
        service_(service) {
    DCHECK(!*is_shutdown_);
    service_->RequestSetUrlRewriteRules(&ctx_, &request_, &responder_, cq_, cq_,
                                        static_cast<GRPC*>(this));
  }

  cast::v2::RuntimeApplicationService::AsyncService* service() {
    return service_;
  }

  base::WeakPtr<RuntimeApplicationServiceDelegate> delegate() {
    return delegate_;
  }

  bool* is_shutdown() { return is_shutdown_; }

  void DoMethod() { delegate_->SetUrlRewriteRules(request_, &response_, this); }

 private:
  bool* is_shutdown_;
  base::WeakPtr<RuntimeApplicationServiceDelegate> delegate_;
  cast::v2::RuntimeApplicationService::AsyncService* service_;
};

}  // namespace

RuntimeApplicationServiceDelegate::~RuntimeApplicationServiceDelegate() =
    default;

void StartRuntimeApplicationServiceMethods(
    cast::v2::RuntimeApplicationService::AsyncService* service,
    base::WeakPtr<RuntimeApplicationServiceDelegate> delegate,
    ::grpc::ServerCompletionQueue* cq,
    bool* is_shutdown) {
  new SetUrlRewriteRules(service, delegate, cq, is_shutdown);
}

}  // namespace chromecast
