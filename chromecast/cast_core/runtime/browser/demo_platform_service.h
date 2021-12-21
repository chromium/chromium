// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_DEMO_PLATFORM_SERVICE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_DEMO_PLATFORM_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "third_party/cast_core/public/src/proto/core/cast_core_service.grpc.pb.h"
#include "third_party/cast_core/public/src/proto/platform/platform_service.grpc.pb.h"

namespace cast {
namespace platform {

class DemoPlatformService final : public PlatformService::Service,
                                  public base::PlatformThread::Delegate {
 public:
  explicit DemoPlatformService(base::FilePath exe_dir);
  ~DemoPlatformService() override;

  void RunLoop();

  // PlatformService::Service implementation:
  grpc::Status StartRuntime(grpc::ServerContext* context,
                            const StartRuntimeRequest* request,
                            StartRuntimeResponse* response) override;
  grpc::Status StopRuntime(grpc::ServerContext* context,
                           const StopRuntimeRequest* request,
                           StopRuntimeResponse* response) override;
  grpc::Status GetDeviceInfo(grpc::ServerContext* context,
                             const GetDeviceInfoRequest* request,
                             GetDeviceInfoResponse* response) override;

 private:
  void RegisterRuntimeOnSequence();
  void StartRuntimeOnSequence(int tee_pipe0,
                              int tee_pipe1,
                              const std::string& runtime_service_path);
  void StopRuntimeOnSequence();

  // base::PlatformThread::Delegate implementation:
  void ThreadMain() override;

  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_;
  scoped_refptr<base::sequence_manager::TaskQueue> task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::FilePath exe_dir_;
  std::unique_ptr<cast::core::CastCoreService::Stub> cast_core_stub_;
  std::string runtime_id_;
  base::Process runtime_process_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace platform
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_DEMO_PLATFORM_SERVICE_H_
