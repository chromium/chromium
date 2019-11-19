// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_RPC_INSTANCE_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_RPC_INSTANCE_H_

#include "chromecast/browser/webview/platform_views_rpc_instance.h"

namespace chromecast {

class WebviewRpcInstance : public PlatformViewsRpcInstance {
 public:
  WebviewRpcInstance(webview::WebviewService::AsyncService* service,
                     grpc::ServerCompletionQueue* cq,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     WebviewWindowManager* window_manager);
  WebviewRpcInstance(webview::PlatformViewsService::AsyncService* service,
                     grpc::ServerCompletionQueue* cq,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     WebviewWindowManager* window_manager);
  ~WebviewRpcInstance() override;

 protected:
  void CreateNewInstance() override;
  bool Initialize() override;

 private:
  void CreateWebview(int app_id, int window_id);
  webview::WebviewService::AsyncService* webview_service_;
  webview::PlatformViewsService::AsyncService* platform_views_service_;

  DISALLOW_COPY_AND_ASSIGN(WebviewRpcInstance);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_RPC_INSTANCE_H_
