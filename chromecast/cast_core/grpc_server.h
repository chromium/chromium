// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_SERVER_H_
#define CHROMECAST_CAST_CORE_GRPC_SERVER_H_

#include <atomic>
#include <memory>

#include "base/sequenced_task_runner.h"
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

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Unowned pointer to the completion queue so it can be shut down.
  grpc::ServerCompletionQueue* grpc_cq_{nullptr};

  std::unique_ptr<grpc::Server> grpc_server_;

 private:
  // base::PlatformThread::Delegate implementation:
  void ThreadMain() override;

  // Allows ownership of the completion queue to be passed to the gRPC thread.
  std::atomic<grpc::ServerCompletionQueue*> grpc_cq_owned_{nullptr};

  base::PlatformThreadHandle grpc_thread_handle_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_GRPC_SERVER_H_
