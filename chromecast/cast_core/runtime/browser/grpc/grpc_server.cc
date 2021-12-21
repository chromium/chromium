// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/grpc/grpc_server.h"

#include "chromecast/cast_core/runtime/browser/grpc/grpc_method.h"

namespace chromecast {

GrpcServer::GrpcServer(scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  server_objects_.store(new ServerObjects);
}

GrpcServer::~GrpcServer() {
  delete server_objects_.load();
}

void GrpcServer::Start() {
  DCHECK(grpc_cq_);
  DCHECK(grpc_server_);
  base::PlatformThread::Create(0, this, &grpc_thread_handle_);
  base::PlatformThread::Detach(grpc_thread_handle_);
}

void GrpcServer::Stop() {
  if (grpc_cq_) {
    is_shutdown_ = true;
    // NOTE: We pass a deadline of 0 because we don't need any pending calls to
    // be allowed to finish, and more importantly they won't.  Since requests
    // are also processed on this sequence, we can't block this sequence waiting
    // for them.
    gpr_timespec deadline = {};
    deadline.clock_type = GPR_TIMESPAN;
    grpc_server_->Shutdown(deadline);
    // NOTE: This leads to cq->Next() returning false.
    grpc_cq_->Shutdown();
    grpc_server_ = nullptr;
    grpc_cq_ = nullptr;
  }
}

void GrpcServer::SetCompletionQueue(
    std::unique_ptr<grpc::ServerCompletionQueue> cq) {
  auto* objects = server_objects_.load();
  DCHECK(cq);
  DCHECK(objects);
  DCHECK(!objects->cq);
  grpc_cq_ = cq.get();
  objects->cq = std::move(cq);
}

void GrpcServer::SetServer(std::unique_ptr<grpc::Server> server) {
  auto* objects = server_objects_.load();
  DCHECK(server);
  DCHECK(objects);
  DCHECK(!objects->server);
  grpc_server_ = server.get();
  objects->server = std::move(server);
}

GrpcServer::ServerObjects::ServerObjects() = default;
GrpcServer::ServerObjects::~ServerObjects() = default;

void GrpcServer::ThreadMain() {
  // NOTE: A dedicated thread is all but required by gRPC, even when switching
  // to async because CompletionQueue's Next() is blocking and AsyncNext() would
  // rely on repeated polling which probably isn't friendly to a TaskRunner.
  base::PlatformThread::SetName("GRPCServer");

  // NOTE: This thread owns the CQ and server now and will delete them after a
  // shutdown triggered by Stop().  Even after the Shutdown() calls and the CQ
  // is draining, it can still access some fields of the server, so both have to
  // be kept alive by this thread.
  std::unique_ptr<ServerObjects> server_objects{
      server_objects_.exchange(nullptr)};
  grpc::ServerCompletionQueue* cq = server_objects->cq.get();

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
