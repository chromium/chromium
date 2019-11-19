// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/platform_views_rpc_instance.h"

#include <deque>
#include <mutex>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/browser/webview/proto/webview.grpc.pb.h"
#include "chromecast/browser/webview/webview_window_manager.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace chromecast {

PlatformViewsRpcInstance::PlatformViewsRpcInstance(
    grpc::ServerCompletionQueue* cq,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    WebviewWindowManager* window_manager)
    : cq_(cq),
      io_(&ctx_),
      window_manager_(window_manager),
      task_runner_(task_runner) {
  write_options_.clear_buffer_hint();
  write_options_.clear_corked();

  init_callback_ = base::BindRepeating(&PlatformViewsRpcInstance::InitComplete,
                                       base::Unretained(this));
  read_callback_ = base::BindRepeating(&PlatformViewsRpcInstance::ReadComplete,
                                       base::Unretained(this));
  write_callback_ = base::BindRepeating(
      &PlatformViewsRpcInstance::WriteComplete, base::Unretained(this));
  destroy_callback_ = base::BindRepeating(
      &PlatformViewsRpcInstance::FinishComplete, base::Unretained(this));
}

PlatformViewsRpcInstance::~PlatformViewsRpcInstance() {
  DCHECK(destroying_);
  if (controller_) {
    controller_.release()->Destroy();
  }

  window_manager_->RemoveObserver(this);
}

void PlatformViewsRpcInstance::FinishComplete(bool ok) {
  // Bounce off of the webview thread to allow it to complete any pending work.
  destroying_ = true;
  if (!send_pending_) {
    task_runner_->DeleteSoon(FROM_HERE, this);
  }
}

void PlatformViewsRpcInstance::ProcessRequestOnControllerThread(
    std::unique_ptr<webview::WebviewRequest> request) {
  controller_->ProcessRequest(*request.get());
}

void PlatformViewsRpcInstance::InitComplete(bool ok) {
  if (!ok) {
    destroying_ = true;
    delete this;
    return;
  }

  request_ = std::make_unique<webview::WebviewRequest>();
  io_.Read(request_.get(), &read_callback_);

  // Create a new instance to handle new requests.
  // Instances of this class delete themselves.
  CreateNewInstance();
}

void PlatformViewsRpcInstance::ReadComplete(bool ok) {
  if (!ok) {
    io_.Finish(grpc::Status(), &destroy_callback_);
  } else if (controller_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PlatformViewsRpcInstance::ProcessRequestOnControllerThread,
            base::Unretained(this), base::Passed(std::move(request_))));

    request_ = std::make_unique<webview::WebviewRequest>();
    std::unique_lock<std::mutex> l(send_lock_);
    if (!errored_)
      io_.Read(request_.get(), &read_callback_);
  } else if (!Initialize()) {
    io_.Finish(grpc::Status(grpc::FAILED_PRECONDITION, "Failed initialization"),
               &destroy_callback_);
  }
  // In this case initialization is pending and the main webview thread will
  // start the first read.
}

void PlatformViewsRpcInstance::WriteComplete(bool ok) {
  std::unique_lock<std::mutex> l(send_lock_);
  send_pending_ = false;
  if (destroying_) {
    // It is possible for the read & finish to complete while a write is
    // outstanding, in that case just re-call it to delete this instance.
    FinishComplete(true);
  } else if (errored_) {
    io_.Finish(grpc::Status(grpc::UNKNOWN, error_message_), &destroy_callback_);
  } else if (!pending_messages_.empty()) {
    send_pending_ = true;
    io_.Write(*pending_messages_.front().get(), write_options_,
              &write_callback_);
    pending_messages_.pop_front();
  }
}

void PlatformViewsRpcInstance::EnqueueSend(
    std::unique_ptr<webview::WebviewResponse> response) {
  std::unique_lock<std::mutex> l(send_lock_);
  if (errored_ || destroying_)
    return;
  if (!send_pending_ && pending_messages_.empty()) {
    send_pending_ = true;
    io_.Write(*response.get(), write_options_, &write_callback_);
  } else {
    pending_messages_.emplace_back(std::move(response));
  }
}

void PlatformViewsRpcInstance::OnError(const std::string& error_message) {
  std::unique_lock<std::mutex> l(send_lock_);
  errored_ = true;
  error_message_ = error_message;

  if (!send_pending_)
    io_.Finish(grpc::Status(grpc::UNKNOWN, error_message_), &destroy_callback_);
  send_pending_ = true;
}

void PlatformViewsRpcInstance::OnNewWebviewContainerWindow(aura::Window* window,
                                                           int app_id) {
  if (app_id != app_id_)
    return;

  controller_->AttachTo(window, window_id_);
  // The Webview is attached! No reason to keep on listening for window property
  // updates.
  window_manager_->RemoveObserver(this);
}

}  // namespace chromecast
