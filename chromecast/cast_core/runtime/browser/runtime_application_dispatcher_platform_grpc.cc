// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher_platform_grpc.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_platform.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_platform_grpc.h"
#include "chromecast/cast_core/runtime/browser/streaming_runtime_application.h"
#include "chromecast/cast_core/runtime/browser/web_runtime_application.h"
#include "third_party/cast_core/public/src/proto/common/application_config.pb.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"
#include "third_party/openscreen/src/cast/common/public/cast_streaming_app_ids.h"

namespace chromecast {
namespace {

constexpr base::TimeDelta kDefaultMetricsReportInterval = base::Seconds(60);

std::unique_ptr<RuntimeApplicationPlatform>
CreateRuntimeApplicationPlatformFactory(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::string session_id,
    RuntimeApplicationPlatform::Client& client) {
  return std::make_unique<RuntimeApplicationPlatformGrpc>(
      std::move(task_runner), std::move(session_id), client);
}

}  // namespace

RuntimeApplicationDispatcherPlatformGrpc::
    RuntimeApplicationDispatcherPlatformGrpc(
        RuntimeApplicationDispatcherPlatform::Client& client,
        CastWebService* web_service,
        CastRuntimeMetricsRecorder::EventBuilderFactory* event_builder_factory,
        std::string runtime_id,
        std::string runtime_service_endpoint)
    : client_(client),
      runtime_id_(std::move(runtime_id)),
      runtime_service_endpoint_(std::move(runtime_service_endpoint)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      metrics_recorder_(event_builder_factory) {
  heartbeat_timer_.SetTaskRunner(task_runner_);
}

RuntimeApplicationDispatcherPlatformGrpc::
    ~RuntimeApplicationDispatcherPlatformGrpc() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

bool RuntimeApplicationDispatcherPlatformGrpc::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!grpc_server_);

  LOG(INFO) << "Starting runtime service: runtime_id=" << runtime_id_
            << ", endpoint=" << runtime_service_endpoint_;

  grpc_server_.emplace();
  grpc_server_->SetHandler<
      cast::runtime::RuntimeServiceHandler::LoadApplication>(base::BindPostTask(
      task_runner_,
      base::BindRepeating(
          &RuntimeApplicationDispatcherPlatformGrpc::HandleLoadApplication,
          weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::LaunchApplication>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(&RuntimeApplicationDispatcherPlatformGrpc::
                                      HandleLaunchApplication,
                                  weak_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<
      cast::runtime::RuntimeServiceHandler::StopApplication>(base::BindPostTask(
      task_runner_,
      base::BindRepeating(
          &RuntimeApplicationDispatcherPlatformGrpc::HandleStopApplication,
          weak_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<cast::runtime::RuntimeServiceHandler::Heartbeat>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(
              &RuntimeApplicationDispatcherPlatformGrpc::HandleHeartbeat,
              weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::StartMetricsRecorder>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(&RuntimeApplicationDispatcherPlatformGrpc::
                                      HandleStartMetricsRecorder,
                                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::StopMetricsRecorder>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(&RuntimeApplicationDispatcherPlatformGrpc::
                                      HandleStopMetricsRecorder,
                                  weak_factory_.GetWeakPtr())));
  grpc_server_->Start(runtime_service_endpoint_);

  LOG(INFO) << "Runtime service started";
  return true;
}

void RuntimeApplicationDispatcherPlatformGrpc::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (heartbeat_reactor_) {
    heartbeat_timer_.Stop();
    heartbeat_reactor_->Write(grpc::Status::OK);
    heartbeat_reactor_ = nullptr;
  }

  if (metrics_recorder_service_) {
    metrics_recorder_service_->OnCloseSoon(base::DoNothing());
    metrics_recorder_service_.reset();
  }

  if (grpc_server_) {
    grpc_server_->Stop();
    grpc_server_.reset();
    LOG(INFO) << "Runtime service stopped";
  }
}

void RuntimeApplicationDispatcherPlatformGrpc::HandleLoadApplication(
    cast::runtime::LoadApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (request.cast_session_id().empty()) {
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Application session ID is missing"));
    return;
  }

  std::string session_id = request.cast_session_id();
  if (client_->HasApplication(session_id)) {
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Application already exists"));
    return;
  }

  if (!request.has_application_config()) {
    reactor->Write(
        grpc::Status(grpc::INVALID_ARGUMENT, "Application config is missing"));
    return;
  }

  client_->LoadApplication(
      std::move(request),
      base::BindPostTask(
          task_runner_,
          base::BindOnce(
              &RuntimeApplicationDispatcherPlatformGrpc::OnApplicationLoaded,
              weak_factory_.GetWeakPtr(), std::move(session_id),
              std::move(reactor))),
      base::BindOnce(&CreateRuntimeApplicationPlatformFactory));
}

void RuntimeApplicationDispatcherPlatformGrpc::HandleLaunchApplication(
    cast::runtime::LaunchApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_->LaunchApplication(
      std::move(request),
      base::BindPostTask(
          task_runner_,
          base::BindOnce(
              &RuntimeApplicationDispatcherPlatformGrpc::OnApplicationLaunching,
              weak_factory_.GetWeakPtr(), request.cast_session_id(),
              std::move(reactor))));
}

void RuntimeApplicationDispatcherPlatformGrpc::HandleStopApplication(
    cast::runtime::StopApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::StopApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<RuntimeApplication> app =
      client_->StopApplication(request.cast_session_id());
  if (!app) {
    LOG(ERROR) << "Application doesn't exist anymore: session_id"
               << request.cast_session_id();
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Application not found"));
    return;
  }

  // Reset the app only after the response is constructed.
  cast::runtime::StopApplicationResponse response;
  response.set_app_id(app->GetAppId());
  response.set_cast_session_id(app->GetCastSessionId());
  reactor->Write(std::move(response));
}

void RuntimeApplicationDispatcherPlatformGrpc::HandleHeartbeat(
    cast::runtime::HeartbeatRequest request,
    cast::runtime::RuntimeServiceHandler::Heartbeat::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!heartbeat_reactor_);

  if (!request.has_heartbeat_period() ||
      request.heartbeat_period().seconds() <= 0) {
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Incorrect heartbeat period"));
    return;
  }

  heartbeat_period_ = base::Seconds(request.heartbeat_period().seconds());
  heartbeat_reactor_ = reactor;
  heartbeat_reactor_->SetWritesAvailableCallback(base::BindPostTask(
      task_runner_,
      base::BindRepeating(
          &RuntimeApplicationDispatcherPlatformGrpc::OnHeartbeatSent,
          weak_factory_.GetWeakPtr())));
  DVLOG(2) << "Starting heartbeat ticking with period: " << heartbeat_period_
           << " seconds";

  SendHeartbeat();
}

void RuntimeApplicationDispatcherPlatformGrpc::HandleStartMetricsRecorder(
    cast::runtime::StartMetricsRecorderRequest request,
    cast::runtime::RuntimeServiceHandler::StartMetricsRecorder::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request.metrics_recorder_service_info().grpc_endpoint().empty()) {
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "MetricsRecord service endpoint is missing"));
    return;
  }

  metrics_recorder_stub_.emplace(
      request.metrics_recorder_service_info().grpc_endpoint());
  metrics_recorder_service_.emplace(
      &metrics_recorder_, &action_recorder_,
      base::BindRepeating(
          &RuntimeApplicationDispatcherPlatformGrpc::RecordMetrics,
          weak_factory_.GetWeakPtr()),
      kDefaultMetricsReportInterval);
  DVLOG(2) << "MetricsRecorderService connected: endpoint="
           << request.metrics_recorder_service_info().grpc_endpoint();
  reactor->Write(cast::runtime::StartMetricsRecorderResponse());
}

void RuntimeApplicationDispatcherPlatformGrpc::HandleStopMetricsRecorder(
    cast::runtime::StopMetricsRecorderRequest request,
    cast::runtime::RuntimeServiceHandler::StopMetricsRecorder::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics_recorder_service_->OnCloseSoon(
      base::BindOnce(&RuntimeApplicationDispatcherPlatformGrpc::
                         OnMetricsRecorderServiceStopped,
                     weak_factory_.GetWeakPtr(), std::move(reactor)));
}

void RuntimeApplicationDispatcherPlatformGrpc::OnApplicationLoaded(
    std::string session_id,
    cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor,
    cast_receiver::Status success) {
  if (!client_->HasApplication(session_id)) {
    LOG(ERROR) << "Application doesn't exist anymore: session_id" << session_id;
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Application not found"));
    return;
  }

  if (!success) {
    reactor->Write(
        grpc::Status(grpc::StatusCode::UNKNOWN, "Failed to load application"));
    return;
  }

  cast::runtime::LoadApplicationResponse response;
  response.mutable_message_port_info();
  reactor->Write(std::move(response));
}

void RuntimeApplicationDispatcherPlatformGrpc::OnApplicationLaunching(
    std::string session_id,
    cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor,
    cast_receiver::Status success) {
  if (!client_->HasApplication(session_id)) {
    LOG(ERROR) << "Application doesn't exist anymore: session_id" << session_id;
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Application not found"));
    return;
  }

  if (!success) {
    reactor->Write(grpc::Status(grpc::StatusCode::UNKNOWN,
                                "Failed to launch application"));
    return;
  }

  reactor->Write(cast::runtime::LaunchApplicationResponse());
}

void RuntimeApplicationDispatcherPlatformGrpc::SendHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(heartbeat_reactor_);
  DVLOG(2) << "Sending heartbeat";
  heartbeat_reactor_->Write(cast::runtime::HeartbeatResponse());
}

void RuntimeApplicationDispatcherPlatformGrpc::OnHeartbeatSent(
    cast::utils::GrpcStatusOr<
        cast::runtime::RuntimeServiceHandler::Heartbeat::Reactor*> reactor_or) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!reactor_or.ok()) {
    heartbeat_reactor_ = nullptr;
    LOG(ERROR) << "Failed to send heartbeats: " << reactor_or.ToString();
    return;
  }

  heartbeat_reactor_ = std::move(reactor_or).value();
  heartbeat_timer_.Start(
      FROM_HERE, heartbeat_period_,
      base::BindPostTask(
          task_runner_,
          base::BindOnce(
              &RuntimeApplicationDispatcherPlatformGrpc::SendHeartbeat,
              weak_factory_.GetWeakPtr())));
}

void RuntimeApplicationDispatcherPlatformGrpc::RecordMetrics(
    cast::metrics::RecordRequest request,
    CastRuntimeMetricsRecorderService::RecordCompleteCallback
        record_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metrics_recorder_stub_) {
    std::move(record_complete_callback).Run();
    return;
  }

  auto call =
      metrics_recorder_stub_
          ->CreateCall<cast::metrics::MetricsRecorderServiceStub::Record>(
              std::move(request));
  std::move(call).InvokeAsync(base::BindPostTask(
      task_runner_,
      base::BindOnce(
          &RuntimeApplicationDispatcherPlatformGrpc::OnMetricsRecorded,
          weak_factory_.GetWeakPtr(), std::move(record_complete_callback))));
}

void RuntimeApplicationDispatcherPlatformGrpc::OnMetricsRecorded(
    CastRuntimeMetricsRecorderService::RecordCompleteCallback
        record_complete_callback,
    cast::utils::GrpcStatusOr<cast::metrics::RecordResponse> response_or) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response_or.ok()) {
    LOG(ERROR) << "Failed to record metrics: " << response_or.ToString();
  }

  std::move(record_complete_callback).Run();
}

void RuntimeApplicationDispatcherPlatformGrpc::OnMetricsRecorderServiceStopped(
    cast::runtime::RuntimeServiceHandler::StopMetricsRecorder::Reactor*
        reactor) {
  DVLOG(2) << "MetricsRecorderService stopped";
  metrics_recorder_service_.reset();
  reactor->Write(cast::runtime::StopMetricsRecorderResponse());
}

}  // namespace chromecast
