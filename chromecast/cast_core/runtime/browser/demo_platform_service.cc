// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/demo_platform_service.h"

#include <stdlib.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/grpc/src/include/grpcpp/server.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace {

constexpr const char* kCastCoreServiceAddress =
    "unix:/tmp/cast/grpc/core-service";
constexpr const char* kPlatformServiceAddress =
    "unix:/tmp/cast/grpc/platform-service";

}  // namespace

namespace cast {
namespace platform {

DemoPlatformService::DemoPlatformService(base::FilePath exe_dir)
    : sequence_manager_(
          base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
              base::MessagePump::Create(base::MessagePumpType::DEFAULT),
              base::sequence_manager::SequenceManager::Settings::Builder()
                  .SetMessagePumpType(base::MessagePumpType::DEFAULT)
                  .Build())),
      task_queue_(sequence_manager_->CreateTaskQueue(
          base::sequence_manager::TaskQueue::Spec("platform service tasks"))),
      task_runner_(task_queue_->task_runner()),
      exe_dir_(exe_dir) {
  auto cast_core_channel = grpc::CreateChannel(
      kCastCoreServiceAddress, grpc::InsecureChannelCredentials());

  if (!cast_core_channel) {
    LOG(ERROR) << "Failed to connect to cast core grpc channel";
    return;
  }

  cast_core_stub_ =
      cast::core::CastCoreService::NewStub(std::move(cast_core_channel));
}

DemoPlatformService::~DemoPlatformService() = default;

void DemoPlatformService::RunLoop() {
  base::ThreadTaskRunnerHandle thread_handle(task_runner_);
  base::RunLoop().Run();
}

grpc::Status DemoPlatformService::StartRuntime(
    grpc::ServerContext* context,
    const StartRuntimeRequest* request,
    StartRuntimeResponse* response) {
  LOG(INFO) << "StartRuntime";
  int tee_pipe[2];
  if (pipe(tee_pipe) != 0) {
    LOG(ERROR) << "Can't create a pipe";
    return grpc::Status::CANCELLED;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DemoPlatformService::StartRuntimeOnSequence,
                     base::Unretained(this), tee_pipe[0], tee_pipe[1],
                     request->runtime_service_info().grpc_endpoint()));
  return grpc::Status::OK;
}

grpc::Status DemoPlatformService::StopRuntime(grpc::ServerContext* context,
                                              const StopRuntimeRequest* request,
                                              StopRuntimeResponse* response) {
  LOG(INFO) << "StopRuntime";
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DemoPlatformService::StopRuntimeOnSequence,
                                base::Unretained(this)));
  return grpc::Status::OK;
}

grpc::Status DemoPlatformService::GetDeviceInfo(
    grpc::ServerContext* context,
    const GetDeviceInfoRequest* request,
    GetDeviceInfoResponse* response) {
  LOG(INFO) << "GetDeviceInfo";
  return grpc::Status::OK;
}

void DemoPlatformService::ThreadMain() {
  base::PlatformThread::SetName("gRPCThread");

  if (!cast_core_stub_) {
    return;
  }

  grpc::ServerBuilder builder;
  builder.AddListeningPort(kPlatformServiceAddress,
                           grpc::InsecureServerCredentials());
  builder.RegisterService(this);
  std::unique_ptr<grpc::Server> grpc_server = builder.BuildAndStart();

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DemoPlatformService::RegisterRuntimeOnSequence,
                                base::Unretained(this)));

  LOG(INFO) << "PlatformService initialized, waiting for start request";
  grpc_server->Wait();
}

void DemoPlatformService::RegisterRuntimeOnSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  grpc::ClientContext context;
  cast::core::RegisterRuntimeRequest register_request;
  cast::common::RuntimeMetadata* metadata =
      register_request.mutable_runtime_metadata();
  base::FilePath exe_path = exe_dir_.Append("core_runtime");
  metadata->set_name(exe_path.value());
  metadata->set_type(cast::common::RuntimeType_Type_CAST_WEB);
  cast::common::RuntimeCapabilities* runtime_capabilities =
      metadata->mutable_runtime_capabilities();
  runtime_capabilities->mutable_media_capabilities()->set_audio_supported(true);
  runtime_capabilities->mutable_media_capabilities()->set_video_supported(true);
  runtime_capabilities->set_metrics_recorder_supported(true);

  cast::core::RegisterRuntimeResponse register_response;
  grpc::Status status = cast_core_stub_->RegisterRuntime(
      &context, register_request, &register_response);
  if (!status.ok()) {
    LOG(FATAL) << "Failed to register runtime: " << status.error_message();
  } else {
    runtime_id_ = register_response.runtime_id();
    CHECK(!runtime_id_.empty()) << "Core returned an empty runtime ID";
  }
}

void DemoPlatformService::StartRuntimeOnSequence(
    int tee_pipe0,
    int tee_pipe1,
    const std::string& runtime_service_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::LaunchOptions tee_options;
  base::LaunchOptions runtime_options;
  tee_options.fds_to_remap.emplace_back(tee_pipe0, STDIN_FILENO);
  runtime_options.fds_to_remap.emplace_back(tee_pipe1, STDOUT_FILENO);
  runtime_options.fds_to_remap.emplace_back(tee_pipe1, STDERR_FILENO);

  base::CommandLine tee_cmdline(base::FilePath("tee"));
  const char* log_filename = getenv("DEMO_RUNTIME_LOG");
  if (!log_filename) {
    log_filename = "runtime_log.txt";
  }
  tee_cmdline.AppendArg(log_filename);
  LOG(INFO) << "Logging runtime to file: " << log_filename;
  base::LaunchProcess(tee_cmdline, tee_options);

  base::CommandLine runtime_cmdline(exe_dir_.Append("core_runtime"));
  runtime_cmdline.AppendSwitch("--no-sandbox");
  runtime_cmdline.AppendSwitch("--no-wifi");
#if !defined(__arm__)
  runtime_cmdline.AppendSwitchASCII("--ozone-platform", "x11");
#endif
  runtime_cmdline.AppendSwitchASCII("--log-file",
                                    "./runtime_logs/" + runtime_id_ + ".log");
  runtime_cmdline.AppendSwitchASCII("--vmodule", "*runtime_*=2");
  runtime_cmdline.AppendSwitchASCII("--cast-core-runtime-id", runtime_id_);
  runtime_cmdline.AppendSwitchASCII("--runtime-service-path",
                                    runtime_service_path);
  // TODO(btolsch): Maybe support forwarding flags after '--' to the runtime.
  runtime_process_ = base::LaunchProcess(runtime_cmdline, runtime_options);

  close(tee_pipe0);
  close(tee_pipe1);
}

void DemoPlatformService::StopRuntimeOnSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool terminated = runtime_process_.Terminate(0, true);
  LOG_IF(WARNING, !terminated)
      << "Unable to terminate runtime process before timeout.";
}

}  // namespace platform
}  // namespace cast

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_STDERR;
  logging::InitLogging(logging_settings);

  base::FilePath exe_dir;
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    LOG(ERROR) << "Couldn't get our path for launching the runtime.";
    return 1;
  }
  cast::platform::DemoPlatformService platform_service(exe_dir);

  base::PlatformThreadHandle thread_handle;
  base::PlatformThread::Create(0, &platform_service, &thread_handle);
  base::PlatformThread::Detach(thread_handle);
  platform_service.RunLoop();

  return 0;
}
