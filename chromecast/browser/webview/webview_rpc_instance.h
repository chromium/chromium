// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_RPC_INSTANCE_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_RPC_INSTANCE_H_

#include "chromecast/browser/webview/platform_views_rpc_instance.h"

namespace chromecast {

class WebviewRpcInstance : public PlatformViewsRpcInstance {
 public:
  WebviewRpcInstance(webview::PlatformViewsService::AsyncService* service,
                     grpc::ServerCompletionQueue* cq,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     WebviewWindowManager* window_manager,
                     bool enabled_for_dev);

  WebviewRpcInstance(const WebviewRpcInstance&) = delete;
  WebviewRpcInstance& operator=(const WebviewRpcInstance&) = delete;

  ~WebviewRpcInstance() override;

 protected:
  void CreateNewInstance() override;
  bool Initialize() override;

 private:
  void CreateWebview(int app_id, int window_id, bool incognito);
  webview::PlatformViewsService::AsyncService* platform_views_service_;
  bool enabled_for_dev_ = false;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_RPC_INSTANCE_H_
