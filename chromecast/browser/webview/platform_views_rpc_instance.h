// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_PLATFORM_VIEWS_RPC_INSTANCE_H_
#define CHROMECAST_BROWSER_WEBVIEW_PLATFORM_VIEWS_RPC_INSTANCE_H_

#include <deque>
#include <mutex>

#include "base/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/browser/webview/proto/webview.grpc.pb.h"
#include "chromecast/browser/webview/web_content_controller.h"
#include "chromecast/browser/webview/webview_window_manager.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace chromecast {

typedef base::RepeatingCallback<void(bool)> GrpcCallback;

// Threading model and life cycle.
// PlatformViewsRpcInstance creates copies of itself and deletes itself as
// needed. Instances are deleted when the GRPC Finish request completes and
// there are no other outstanding read or write operations. Deletion bounces off
// the webview thread to synchronize with request processing.
class PlatformViewsRpcInstance : public WebContentController::Client,
                                 public WebviewWindowManager::Observer {
 public:
  PlatformViewsRpcInstance(
      grpc::ServerCompletionQueue* cq,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      WebviewWindowManager* window_manager);

  PlatformViewsRpcInstance(const PlatformViewsRpcInstance&) = delete;
  PlatformViewsRpcInstance& operator=(const PlatformViewsRpcInstance&) = delete;

  ~PlatformViewsRpcInstance() override;

 protected:
  virtual void CreateNewInstance() = 0;
  virtual bool Initialize() = 0;

  // WebContentController::Client:
  void OnError(const std::string& error_message) override;

  grpc::ServerContext ctx_;
  grpc::ServerCompletionQueue* cq_;
  std::unique_ptr<webview::WebviewRequest> request_;
  grpc::ServerAsyncReaderWriter<webview::WebviewResponse,
                                webview::WebviewRequest>
      io_;
  std::unique_ptr<WebContentController> controller_;
  GrpcCallback init_callback_;
  GrpcCallback read_callback_;
  WebviewWindowManager* window_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Initialize to negative values since the aura::Window properties use 0 as
  // the default value if the property isn't found.
  int app_id_ = -1;
  int window_id_ = -1;

 private:
  void InitComplete(bool ok);
  void ReadComplete(bool ok);
  void WriteComplete(bool ok);
  void FinishComplete(bool ok);

  void StartRead();
  void CreateWebview(int app_id, int window_id);

  void ProcessRequestOnControllerThread(
      std::unique_ptr<webview::WebviewRequest> request);

  // WebContentController::Client:
  void EnqueueSend(std::unique_ptr<webview::WebviewResponse> response) override;

  // WebviewWindowManager::Observer:
  void OnNewWebviewContainerWindow(aura::Window* window, int app_id) override;

  GrpcCallback write_callback_;
  GrpcCallback destroy_callback_;

  std::mutex send_lock_;
  bool errored_ = false;
  std::string error_message_;
  bool send_pending_ = false;
  // Reads are always pending until disabled by the read path.
  bool read_pending_ = true;
  std::deque<std::unique_ptr<webview::WebviewResponse>> pending_messages_;

  grpc::WriteOptions write_options_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_PLATFORM_VIEWS_RPC_INSTANCE_H_
