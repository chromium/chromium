// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_grpc_service.h"

#include "chromecast/browser/webview/webview_rpc_instance.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/server.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace chromecast {

WebviewAsyncService::WebviewAsyncService(
    std::unique_ptr<webview::WebviewService::AsyncService> service,
    std::unique_ptr<grpc::ServerCompletionQueue> cq,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    CastWindowManager* cast_window_manager)
    : ui_task_runner_(std::move(ui_task_runner)),
      cq_(std::move(cq)),
      service_(std::move(service)),
      window_manager_(cast_window_manager) {
  base::PlatformThread::Create(0, this, &rpc_thread_);
}

WebviewAsyncService::~WebviewAsyncService() {
  cq_->Shutdown();
  base::PlatformThread::Join(rpc_thread_);
}

void WebviewAsyncService::ThreadMain() {
  base::PlatformThread::SetName("CastWebviewGrpcMessagePump");

  void* tag;
  bool ok;
  // This self-deletes.
  new WebviewRpcInstance(service_.get(), cq_.get(), ui_task_runner_,
                         &window_manager_);
  // This thread is joined when this service is destroyed.
  while (cq_->Next(&tag, &ok)) {
    reinterpret_cast<GrpcCallback*>(tag)->Run(ok);
  }
}

}  // namespace chromecast
