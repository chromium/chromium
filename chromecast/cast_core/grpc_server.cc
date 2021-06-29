// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc_server.h"

#include "chromecast/cast_core/grpc_method.h"

namespace chromecast {

GrpcServer::GrpcServer(scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

GrpcServer::~GrpcServer() {
  delete grpc_cq_owned_.load();
}

void GrpcServer::Start() {
  DCHECK(grpc_cq_);
  base::PlatformThread::Create(0, this, &grpc_thread_handle_);
  base::PlatformThread::Detach(grpc_thread_handle_);
}

void GrpcServer::Stop() {
  if (grpc_cq_) {
    auto* cq = grpc_cq_;
    grpc_cq_ = nullptr;
    grpc_server_->Shutdown();
    // NOTE: This leads to cq->Next() returning false.
    cq->Shutdown();
    grpc_server_.reset();
  }
}

void GrpcServer::SetCompletionQueue(
    std::unique_ptr<grpc::ServerCompletionQueue> cq) {
  DCHECK(cq);
  DCHECK(!grpc_cq_);
  grpc_cq_ = cq.release();
  grpc_cq_owned_.store(grpc_cq_);
}

void GrpcServer::ThreadMain() {
  // NOTE: A dedicated thread is all but required by gRPC, even when switching
  // to async because CompletionQueue's Next() is blocking and AsyncNext() would
  // rely on repeated polling which probably isn't friendly to a TaskRunner.
  base::PlatformThread::SetName("GRPCServer");

  // NOTE: This thread owns the CQ now and will delete it after a shutdown
  // triggered by Stop().
  std::unique_ptr<grpc::ServerCompletionQueue> cq(
      grpc_cq_owned_.exchange(nullptr));

  void* tag;
  bool running;
  while (cq->Next(&tag, &running)) {
    grpc::Status status =
        running ? grpc::Status::OK
                : grpc::Status(grpc::ABORTED, "server was shutdown");
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GRPC::StepGRPC,
                       base::Unretained(static_cast<GRPC*>(tag)), status));
  }
}

}  // namespace chromecast
