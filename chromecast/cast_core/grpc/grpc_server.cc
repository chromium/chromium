// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/grpc_server.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromecast/cast_core/grpc/grpc_call_options.h"
#include "chromecast/cast_core/grpc/grpc_factory.h"
#include "chromecast/cast_core/grpc/grpc_server_builder.h"

namespace cast {
namespace utils {

namespace {

static const base::TimeDelta kDefaultServerStopTimeout =
    base::Milliseconds(100);

// Stops gRPC server.
static void StopGrpcServer(
    std::unique_ptr<grpc::Server> server,
    std::unique_ptr<ServerReactorTracker> server_reactor_tracker,
    const base::TimeDelta& timeout,
    base::OnceClosure server_stopped_callback) {
  LOG(INFO) << "Shutting down gRPC server with "
            << server_reactor_tracker->active_reactor_count()
            << " active reactors: " << *server_reactor_tracker;

  // The gRPC Reactors are owned by the gRPC framework and are 'pending'
  // unless Reactor::Finish or similar (StartWriteAndFinish) API is called.
  // This Shutdown call makes sure all the finished reactors are deleted via
  // Reactor::OnDone API. As the timeout is reached, all pending reactors are
  // cancelled via Reactor::OnCancel API. Hence, after Shutdow, all pending
  // reactors can be treated as cancelled and manually destroyed.
  auto gpr_timeout = GrpcCallOptions::ToGprTimespec(timeout);
  server->Shutdown(gpr_timeout);

  // As mentioned above, all the pending reactors are now cancelled and must
  // be destroyed by the ServerReactorTracker.
  DCHECK_EQ(server_reactor_tracker->active_reactor_count(), 0UL)
      << "Not all reactors were cancelled: " << *server_reactor_tracker;
  LOG(INFO) << "All active reactors are finished";

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

void GrpcServer::Start(base::StringPiece endpoint) {
  DCHECK(!server_) << "Server is already running";
  DCHECK(server_reactor_tracker_) << "Server was alreadys shutdown";

  auto builder = GrpcFactory::CreateServerBuilder();
  server_ = builder
                ->AddListeningPort(std::string(endpoint),
                                   grpc::InsecureServerCredentials())
                .RegisterCallbackGenericService(this)
                .BuildAndStart();
  DCHECK(server_) << "Failed to start server";
}

void GrpcServer::Stop() {
  if (!server_) {
    LOG(WARNING) << "Grpc server was already stopped";
    return;
  }

  StopGrpcServer(std::move(server_), std::move(server_reactor_tracker_),
                 kDefaultServerStopTimeout, base::DoNothing());
}

void GrpcServer::Stop(const base::TimeDelta& timeout,
                      base::OnceClosure server_stopped_callback) {
  if (!server_) {
    LOG(WARNING) << "Grpc server was already stopped";
    std::move(server_stopped_callback).Run();
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&StopGrpcServer, std::move(server_),
                     std::move(server_reactor_tracker_), timeout,
                     std::move(server_stopped_callback)));
}

void GrpcServer::StopForTesting(const base::TimeDelta& timeout) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  Stop(timeout, run_loop.QuitClosure());
  run_loop.Run();
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
