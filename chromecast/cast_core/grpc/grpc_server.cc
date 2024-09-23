// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/grpc_server.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chromecast/cast_core/grpc/grpc_call_options.h"

namespace cast {
namespace utils {

namespace {

static const auto kDefaultServerStopTimeoutMs = 100;

// Stops gRPC server.
static void StopGrpcServer(
    std::unique_ptr<grpc::Server> server,
    std::unique_ptr<ServerReactorTracker> server_reactor_tracker,
    int64_t timeout_ms,
    base::OnceClosure server_stopped_callback) {
  LOG(INFO) << "Shutting down gRPC server";

  // The gRPC Reactors are owned by the gRPC framework and are 'pending'
  // unless Reactor::Finish or similar (StartWriteAndFinish) API is called.
  // This Shutdown call makes sure all the finished reactors are deleted via
  // Reactor::OnDone API. As the timeout is reached, all pending reactors are
  // cancelled via Reactor::OnCancel API. Hence, after Shutdow, all pending
  // reactors can be treated as cancelled and manually destroyed.
  auto gpr_timeout = GrpcCallOptions::ToGprTimespec(timeout_ms);
  server->Shutdown(gpr_timeout);

  // As mentioned above, all the pending reactors are now cancelled and must
  // be destroyed by the ServerReactorTracker.
  server_reactor_tracker.reset();

  // Finish server shutdown.
  server->Wait();
  server.reset();
  LOG(INFO) << "gRPC server is shut down";

  std::move(server_stopped_callback).Run();
}

}  // namespace

GrpcServer::GrpcServer()
    : server_reactor_tracker_(std::make_unique<ServerReactorTracker>()) {}

GrpcServer::~GrpcServer() {
  DCHECK(!server_) << "gRPC server must be explicitly stopped";
}

grpc::Status GrpcServer::Start(const std::string& endpoint) {
  DCHECK(!server_) << "Server is already running";
  DCHECK(server_reactor_tracker_) << "Server was alreadys shutdown";

  server_ = grpc::ServerBuilder()
                .AddListeningPort(endpoint, grpc::InsecureServerCredentials())
                .RegisterCallbackGenericService(this)
                .BuildAndStart();
  if (!server_) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "can't start gRPC server on " + endpoint);
  }
  LOG(INFO) << "Grpc server started: " << endpoint;
  return grpc::Status::OK;
}

void GrpcServer::Stop() {
  if (!server_) {
    LOG(WARNING) << "Grpc server was already stopped";
    return;
  }

  StopGrpcServer(std::move(server_), std::move(server_reactor_tracker_),
                 kDefaultServerStopTimeoutMs, base::BindOnce([]() {}));
}

void GrpcServer::Stop(int64_t timeout_ms,
                      base::OnceClosure server_stopped_callback) {
  if (!server_) {
    LOG(WARNING) << "Grpc server was already stopped";
    std::move(server_stopped_callback).Run();
    return;
  }

  // Synchronous requests will block gRPC shutdown unless we post shutdown on
  // a different thread.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&StopGrpcServer, std::move(server_),
                     std::move(server_reactor_tracker_), timeout_ms,
                     std::move(server_stopped_callback)));
}

grpc::ServerGenericBidiReactor* GrpcServer::CreateReactor(
    grpc::GenericCallbackServerContext* ctx) {
  auto iter = registered_handlers_.find(ctx->method());
  if (iter != registered_handlers_.end()) {
    DVLOG(1) << "Found a reactor for " << ctx->method();
    return iter->second->CreateReactor(ctx);
  }
  LOG(WARNING) << "No reactor was specified for " << ctx->method()
               << " - falling back to a default unimplemented reactor";
  return grpc::CallbackGenericService::CreateReactor(ctx);
}

}  // namespace utils
}  // namespace cast
