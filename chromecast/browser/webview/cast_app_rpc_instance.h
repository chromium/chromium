// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_CAST_APP_RPC_INSTANCE_H_
#define CHROMECAST_BROWSER_WEBVIEW_CAST_APP_RPC_INSTANCE_H_

#include "chromecast/browser/webview/platform_views_rpc_instance.h"

#include "content/public/browser/web_contents_observer.h"

namespace chromecast {
class WebContentsProvider;

class CastAppRpcInstance : public PlatformViewsRpcInstance,
                           public content::WebContentsObserver {
 public:
  CastAppRpcInstance(webview::PlatformViewsService::AsyncService* service,
                     grpc::ServerCompletionQueue* cq,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     WebviewWindowManager* window_manager,
                     WebContentsProvider* web_contents_provider);
  ~CastAppRpcInstance() override;

 protected:
  void CreateNewInstance() override;
  bool Initialize() override;

 private:
  void CreateCastAppWindowLink(int platform_view_id, int app_window_id);
  void WebContentsDestroyed() override;
  webview::PlatformViewsService::AsyncService* service_;
  WebContentsProvider* web_contents_provider_;

  DISALLOW_COPY_AND_ASSIGN(CastAppRpcInstance);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_CAST_APP_RPC_INSTANCE_H_
