// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_GRPC_SERVER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_GRPC_SERVER_H_

#include <atomic>
#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "third_party/grpc/src/include/grpcpp/completion_queue.h"
#include "third_party/grpc/src/include/grpcpp/server.h"

namespace chromecast {

class GrpcServer : public base::PlatformThread::Delegate {
 public:
  explicit GrpcServer(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~GrpcServer() override;

  void Start();
  void Stop();

 protected:
  void SetCompletionQueue(std::unique_ptr<grpc::ServerCompletionQueue> cq);
  void SetServer(std::unique_ptr<grpc::Server> server);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Indicates that Stop() has been called and no more requests or other server
  // calls should be made.  This can be used by e.g. SimpleAsyncGrpc.
  bool is_shutdown_{false};

  // Unowned pointers to the completion queue and server so they can be shut
  // down by the polling thread.
  grpc::ServerCompletionQueue* grpc_cq_{nullptr};
  grpc::Server* grpc_server_{nullptr};

 private:
  // Owns the gRPC completion queue and server objects so that a single atomic
  // pointer exchange is possible to transfer ownership of both to the gRPC
  // thread.
  struct ServerObjects {
    ServerObjects();
    ~ServerObjects();

    std::unique_ptr<grpc::Server> server;
    std::unique_ptr<grpc::ServerCompletionQueue> cq;
  };

  // base::PlatformThread::Delegate implementation:
  void ThreadMain() override;

  // Allows ownership of the completion queue and server to be passed to the
  // gRPC thread.
  std::atomic<ServerObjects*> server_objects_;

  base::PlatformThreadHandle grpc_thread_handle_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_GRPC_SERVER_H_
