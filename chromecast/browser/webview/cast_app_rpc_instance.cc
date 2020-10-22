// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/cast_app_rpc_instance.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromecast/browser/webview/cast_app_controller.h"
#include "chromecast/browser/webview/platform_views_rpc_instance.h"
#include "chromecast/browser/webview/web_contents_provider.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/server.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace chromecast {

CastAppRpcInstance::CastAppRpcInstance(
    webview::PlatformViewsService::AsyncService* service,
    grpc::ServerCompletionQueue* cq,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    WebviewWindowManager* window_manager,
    base::WeakPtr<WebContentsProvider> web_contents_provider)
    : PlatformViewsRpcInstance(cq, task_runner, window_manager),
      service_(service),
      web_contents_provider_(web_contents_provider) {
  service_->RequestCreateCastAppWindowLink(&ctx_, &io_, cq_, cq_,
                                           &init_callback_);
}

CastAppRpcInstance::~CastAppRpcInstance() {}

void CastAppRpcInstance::CreateNewInstance() {
  new CastAppRpcInstance(service_, cq_, task_runner_, window_manager_,
                         web_contents_provider_);
}

bool CastAppRpcInstance::Initialize() {
  if (request_->type_case() != webview::WebviewRequest::kAssociate)
    return false;

  // This needs to be done on a valid thread.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CastAppRpcInstance::CreateCastAppWindowLink,
                                base::Unretained(this),
                                request_->associate().platform_view_id(),
                                request_->associate().app_window_id()));
  return true;
}

void CastAppRpcInstance::CreateCastAppWindowLink(int platform_view_id,
                                                 int app_window_id) {
  if (!web_contents_provider_) {
    OnError("web_contents_provider_ is null");
    return;
  }

  app_id_ = platform_view_id;
  content::WebContents* web_contents =
      web_contents_provider_->GetWebContents(app_window_id);
  if (!web_contents) {
    OnError("web_contents is null");
    return;
  }
  Observe(web_contents);
  controller_ = std::make_unique<CastAppController>(this, web_contents);
  window_manager_->AddObserver(this);

  // Begin reading again.
  io_.Read(request_.get(), &read_callback_);
}

void CastAppRpcInstance::WebContentsDestroyed() {
  window_manager_->RemoveObserver(this);
  OnError("Web contents destroyed");
}

}  // namespace chromecast
