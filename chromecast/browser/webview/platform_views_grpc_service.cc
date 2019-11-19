// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/platform_views_grpc_service.h"

#include "base/callback.h"
#include "chromecast/browser/webview/cast_app_rpc_instance.h"
#include "chromecast/browser/webview/web_contents_provider.h"
#include "chromecast/browser/webview/webview_rpc_instance.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace chromecast {

PlatformViewsAsyncService::PlatformViewsAsyncService(
    std::unique_ptr<webview::PlatformViewsService::AsyncService> service,
    std::unique_ptr<grpc::ServerCompletionQueue> cq,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    WebContentsProvider* web_contents_provider)
    : ui_task_runner_(std::move(ui_task_runner)),
      cq_(std::move(cq)),
      service_(std::move(service)),
      window_manager_(nullptr),
      web_contents_provider_(web_contents_provider) {
  base::PlatformThread::Create(0, this, &rpc_thread_);
}

PlatformViewsAsyncService::~PlatformViewsAsyncService() {
  base::PlatformThread::Join(rpc_thread_);
}

void PlatformViewsAsyncService::ThreadMain() {
  base::PlatformThread::SetName("CastPlatformViewsGrpcMessagePump");

  void* tag;
  bool ok;
  // These self-delete.
  new CastAppRpcInstance(service_.get(), cq_.get(), ui_task_runner_,
                         &window_manager_, web_contents_provider_);
  new WebviewRpcInstance(service_.get(), cq_.get(), ui_task_runner_,
                         &window_manager_);
  // This thread is joined when this service is destroyed.
  while (cq_->Next(&tag, &ok)) {
    reinterpret_cast<GrpcCallback*>(tag)->Run(ok);
  }
}

}  // namespace chromecast
