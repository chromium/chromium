// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/grpc_server.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
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

GrpcServer::GrpcServer(std::string_view endpoint)
    : endpoint_(endpoint),
      server_reactor_tracker_(std::make_unique<ServerReactorTracker>()) {}

GrpcServer::GrpcServer(GrpcServer&& server) = default;

GrpcServer& GrpcServer::operator=(GrpcServer&& server) = default;

GrpcServer::~GrpcServer() {
  DCHECK(!server_) << "gRPC server must be explicitly stopped";
}

grpc::Status GrpcServer::Start() {
  CHECK(!server_) << "Server is already running";
  CHECK(!endpoint_.empty()) << "Endpoint must be specified";

  DLOG(INFO) << "Starting grpc server on " << endpoint_ << "...";

  // Check to see if the endpoint contains a TCP/IP port definition as a suffix.
  int tcp_port = 0;
  std::optional<std::string> tcp_endpoint_without_port;
  if (!endpoint_.starts_with("unix:") &&
      !endpoint_.starts_with("unix-abstract:")) {
    // Find out if the port is specified in the endpoint, record its value and
    // separate the URI from the port.
    // Technically, we could use the net::IPAddress class to parse the endpoint,
    // but that parser does not support an endpoint with a port and requires the
    // IPv6 address to NOT be enclosed in brackets. Hence, we do a simple
    // string parsing here.
    bool is_ipv6 = std::count(endpoint_.begin(), endpoint_.end(), ':') > 1;
    auto separator_pos = endpoint_.find(is_ipv6 ? "]:" : ":");
    if (separator_pos == std::string::npos) {
      LOG(ERROR) << "Port must always be specified for TCP/IP endpoints: "
                 << endpoint_;
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "TCP port must be specified: " + endpoint_);
    }
    if (is_ipv6) {
      // Skip the ']' character in the IPv6 address.
      separator_pos += 1;
    }
    if (!base::StringToInt(endpoint_.substr(separator_pos + 1), &tcp_port)) {
      LOG(ERROR) << "Failed to parse TCP/IP port: " << endpoint_;
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "TCP port must be a valid number: " + endpoint_);
    }
    tcp_endpoint_without_port = endpoint_.substr(0, separator_pos);
  }

  // The tcp_port is used for TCP/IP endpoints only and is ignored for UDS.
  server_ = grpc::ServerBuilder()
                .AddListeningPort(endpoint_, grpc::InsecureServerCredentials(),
                                  &tcp_port)
                .RegisterCallbackGenericService(this)
                .BuildAndStart();
  if (!server_) {
    LOG(ERROR) << "Failed to start gRPC server on " << endpoint_;
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Failed to start gRPC server on " + endpoint_);
  }

  if (tcp_endpoint_without_port) {
    endpoint_ =
        base::StringPrintf("%s:%d", *tcp_endpoint_without_port, tcp_port);
  }

  DLOG(INFO) << "Grpc server started on " << endpoint_;
  return grpc::Status::OK;
}

grpc::Status GrpcServer::Start(std::string_view endpoint) {
  CHECK(!server_) << "Server is already running";
  endpoint_ = endpoint;
  return Start();
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
