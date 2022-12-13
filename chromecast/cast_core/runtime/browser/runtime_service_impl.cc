// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_service_impl.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/cast_core/cast_core_switches.h"
#include "chromecast/cast_core/runtime/browser/core_conversions.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_service_impl.h"
#include "chromecast/metrics/cast_event_builder_simple.h"
#include "components/cast_receiver/browser/public/content_browser_client_mixins.h"
#include "components/cast_receiver/browser/public/embedder_application.h"
#include "third_party/cast_core/public/src/proto/common/application_config.pb.h"

namespace chromecast {
namespace {

constexpr base::TimeDelta kDefaultMetricsReportInterval = base::Seconds(60);

}  // namespace

RuntimeServiceImpl::RuntimeServiceImpl(
    cast_receiver::ContentBrowserClientMixins& browser_mixins,
    CastWebService& web_service,
    std::string runtime_id,
    std::string runtime_service_endpoint)
    : runtime_id_(std::move(runtime_id)),
      runtime_service_endpoint_(std::move(runtime_service_endpoint)),
      application_dispatcher_(
          browser_mixins
              .CreateApplicationDispatcher<RuntimeApplicationServiceImpl>()),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      web_service_(web_service),
      metrics_recorder_(this) {
  heartbeat_timer_.SetTaskRunner(task_runner_);
}

RuntimeServiceImpl::~RuntimeServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

cast_receiver::Status RuntimeServiceImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!grpc_server_);

  LOG(INFO) << "Starting runtime service: runtime_id=" << runtime_id_
            << ", endpoint=" << runtime_service_endpoint_;

  grpc_server_.emplace();
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::LoadApplication>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(&RuntimeServiceImpl::HandleLoadApplication,
                                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::LaunchApplication>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(&RuntimeServiceImpl::HandleLaunchApplication,
                                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::StopApplication>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(&RuntimeServiceImpl::HandleStopApplication,
                                  weak_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<cast::runtime::RuntimeServiceHandler::Heartbeat>(
      base::BindPostTask(task_runner_, base::BindRepeating(
                                           &RuntimeServiceImpl::HandleHeartbeat,
                                           weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::StartMetricsRecorder>(
          base::BindPostTask(
              task_runner_, base::BindRepeating(
                                &RuntimeServiceImpl::HandleStartMetricsRecorder,
                                weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::StopMetricsRecorder>(
          base::BindPostTask(task_runner_,
                             base::BindRepeating(
                                 &RuntimeServiceImpl::HandleStopMetricsRecorder,
                                 weak_factory_.GetWeakPtr())));
  grpc_server_->Start(runtime_service_endpoint_);

  LOG(INFO) << "Runtime service started";
  return cast_receiver::OkStatus();
}

cast_receiver::Status RuntimeServiceImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  action_recorder_.reset();

  if (heartbeat_reactor_) {
    heartbeat_timer_.Stop();
    // Reset the writes callback as we're not expecting any more responses from
    // gRPC framework.
    heartbeat_reactor_->SetWritesAvailableCallback(base::DoNothing());
    heartbeat_reactor_->Write(grpc::Status::OK);
    heartbeat_reactor_ = nullptr;
  }

  if (metrics_recorder_service_) {
    metrics_recorder_service_->OnCloseSoon(base::DoNothing());
    metrics_recorder_service_.reset();
    metrics_recorder_stub_.reset();
    action_recorder_.reset();
  }

  if (grpc_server_) {
    grpc_server_->Stop();
    grpc_server_.reset();
  }

  LOG(INFO) << "Runtime service stopped";
  return cast_receiver::OkStatus();
}

std::unique_ptr<CastEventBuilder> RuntimeServiceImpl::CreateEventBuilder() {
  return std::make_unique<CastEventBuilderSimple>();
}

void RuntimeServiceImpl::HandleLoadApplication(
    cast::runtime::LoadApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (request.cast_session_id().empty()) {
    LOG(ERROR) << "Session ID is empty";
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Application session ID is missing"));
    return;
  }

  if (application_dispatcher_->GetApplication(request.cast_session_id())) {
    reactor->Write(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                                "Application already exist"));
    return;
  }

  if (!request.has_application_config()) {
    reactor->Write(
        grpc::Status(grpc::INVALID_ARGUMENT, "Application config is missing"));
    return;
  }

  LOG(INFO) << "Loading application: session_id=" << request.cast_session_id();
  RuntimeApplicationServiceImpl* platform_app =
      application_dispatcher_->CreateApplication(
          request.cast_session_id(),
          ToReceiverConfig(request.application_config()),
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                 cast::common::ApplicationConfig config,
                 CastWebService& web_service,
                 std::unique_ptr<cast_receiver::RuntimeApplication>
                     runtime_application) {
                return std::make_unique<RuntimeApplicationServiceImpl>(
                    std::move(runtime_application), std::move(config),
                    std::move(task_runner), web_service);
              },
              task_runner_, std::move(request.application_config()),
              std::ref(*web_service_)));
  platform_app->Load(
      request,
      base::BindPostTask(
          task_runner_,
          base::BindOnce(&RuntimeServiceImpl::OnApplicationLoaded,
                         weak_factory_.GetWeakPtr(), request.cast_session_id(),
                         std::move(reactor))));
}

void RuntimeServiceImpl::HandleLaunchApplication(
    cast::runtime::LaunchApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request.cast_session_id().empty()) {
    LOG(ERROR) << "Session id is empty";
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Sesssion id is missing"));
    return;
  }

  RuntimeApplicationServiceImpl* platform_app =
      application_dispatcher_->GetApplication(request.cast_session_id());
  if (!platform_app) {
    LOG(ERROR) << "Application does not exist";
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Application does not exists"));
    return;
  }

  LOG(INFO) << "Launching application: session_id="
            << request.cast_session_id();
  platform_app->Launch(
      request,
      base::BindPostTask(
          task_runner_,
          base::BindOnce(&RuntimeServiceImpl::OnApplicationLaunching,
                         weak_factory_.GetWeakPtr(), request.cast_session_id(),
                         std::move(reactor))));
}

void RuntimeServiceImpl::HandleStopApplication(
    cast::runtime::StopApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::StopApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request.cast_session_id().empty()) {
    LOG(ERROR) << "Session id is missing";
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Sesssion id is missing"));
    return;
  }

  RuntimeApplicationServiceImpl* platform_app =
      application_dispatcher_->GetApplication(request.cast_session_id());
  if (!platform_app) {
    LOG(ERROR) << "Application doesn't exist anymore: session_id="
               << request.cast_session_id();
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Application not found"));
    return;
  }

  LOG(INFO) << "Stopping application: session_id=" << request.cast_session_id();
  platform_app->Stop(
      request, base::BindOnce(&RuntimeServiceImpl::OnApplicationStopping,
                              weak_factory_.GetWeakPtr(),
                              request.cast_session_id(), std::move(reactor)));
}

void RuntimeServiceImpl::HandleHeartbeat(
    cast::runtime::HeartbeatRequest request,
    cast::runtime::RuntimeServiceHandler::Heartbeat::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!heartbeat_reactor_);

  if (!request.has_heartbeat_period() ||
      request.heartbeat_period().seconds() <= 0) {
    LOG(ERROR) << "Failed to create a heartbeat as period is not valid";
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Incorrect heartbeat period"));
    return;
  }

  heartbeat_period_ = base::Seconds(request.heartbeat_period().seconds());
  heartbeat_reactor_ = reactor;
  // Set the write callback once for all future calls from gRPC framework.
  heartbeat_reactor_->SetWritesAvailableCallback(base::BindPostTask(
      task_runner_, base::BindRepeating(&RuntimeServiceImpl::OnHeartbeatSent,
                                        weak_factory_.GetWeakPtr())));
  DVLOG(2) << "Starting heartbeat ticking with period: " << heartbeat_period_
           << " seconds";

  SendHeartbeat();
}

void RuntimeServiceImpl::HandleStartMetricsRecorder(
    cast::runtime::StartMetricsRecorderRequest request,
    cast::runtime::RuntimeServiceHandler::StartMetricsRecorder::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!action_recorder_);
  DCHECK(!metrics_recorder_stub_);
  DCHECK(!metrics_recorder_service_);

  if (request.metrics_recorder_service_info().grpc_endpoint().empty()) {
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "MetricsRecord service endpoint is missing"));
    return;
  }

  action_recorder_.emplace();
  metrics_recorder_stub_.emplace(
      request.metrics_recorder_service_info().grpc_endpoint());
  metrics_recorder_service_.emplace(
      &metrics_recorder_, &*action_recorder_,
      base::BindRepeating(&RuntimeServiceImpl::RecordMetrics,
                          weak_factory_.GetWeakPtr()),
      kDefaultMetricsReportInterval);
  DVLOG(2) << "MetricsRecorderService connected: endpoint="
           << request.metrics_recorder_service_info().grpc_endpoint();
  reactor->Write(cast::runtime::StartMetricsRecorderResponse());
}

void RuntimeServiceImpl::HandleStopMetricsRecorder(
    cast::runtime::StopMetricsRecorderRequest request,
    cast::runtime::RuntimeServiceHandler::StopMetricsRecorder::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metrics_recorder_service_) {
    LOG(ERROR) << "Droping metrics recorging stop request as service is not "
                  "available anymore";
    reactor->Write(grpc::Status(grpc::StatusCode::UNAVAILABLE,
                                "Metrics recording service is not available"));
    return;
  }

  metrics_recorder_service_->OnCloseSoon(
      base::BindOnce(&RuntimeServiceImpl::OnMetricsRecorderServiceStopped,
                     weak_factory_.GetWeakPtr(), std::move(reactor)));
}

void RuntimeServiceImpl::OnApplicationLoaded(
    std::string session_id,
    cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor,
    cast_receiver::Status status) {
  if (!application_dispatcher_->GetApplication(session_id)) {
    LOG(ERROR) << "Application doesn't exist anymore: session_id="
               << session_id;
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Application not found"));
    return;
  }

  if (!status.ok()) {
    reactor->Write(
        grpc::Status(grpc::StatusCode::UNKNOWN, "Failed to load application"));
    return;
  }

  cast::runtime::LoadApplicationResponse response;
  response.mutable_message_port_info();
  reactor->Write(std::move(response));
}

void RuntimeServiceImpl::OnApplicationLaunching(
    std::string session_id,
    cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor,
    cast_receiver::Status status) {
  if (!application_dispatcher_->GetApplication(session_id)) {
    LOG(ERROR) << "Application doesn't exist anymore: session_id="
               << session_id;
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Application not found"));
    return;
  }

  if (!status.ok()) {
    reactor->Write(grpc::Status(grpc::StatusCode::UNKNOWN,
                                "Failed to launch application"));
    return;
  }

  reactor->Write(cast::runtime::LaunchApplicationResponse());
}

void RuntimeServiceImpl::OnApplicationStopping(
    std::string session_id,
    cast::runtime::RuntimeServiceHandler::StopApplication::Reactor* reactor,
    cast_receiver::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto platform_app = application_dispatcher_->DestroyApplication(session_id);
  DCHECK(platform_app);

  // Reset the app only after the response is constructed.
  cast::runtime::StopApplicationResponse response;
  response.set_app_id(platform_app->app_id());
  response.set_cast_session_id(session_id);
  reactor->Write(std::move(response));
}

void RuntimeServiceImpl::SendHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!heartbeat_reactor_) {
    LOG(WARNING) << "Heartbeat reactor has been destroyed";
    return;
  }

  DVLOG(2) << "Sending heartbeat";
  heartbeat_reactor_->Write(cast::runtime::HeartbeatResponse());
}

void RuntimeServiceImpl::OnHeartbeatSent(
    cast::utils::GrpcStatusOr<
        cast::runtime::RuntimeServiceHandler::Heartbeat::Reactor*> reactor_or) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(heartbeat_reactor_);
  if (!reactor_or.ok()) {
    heartbeat_timer_.Stop();
    heartbeat_reactor_ = nullptr;
    LOG(ERROR) << "Failed to send heartbeats: " << reactor_or.ToString();
    return;
  }

  // Server streaming reactor never changes.
  CHECK(heartbeat_reactor_ == *reactor_or);
  heartbeat_timer_.Start(
      FROM_HERE, heartbeat_period_,
      base::BindPostTask(task_runner_,
                         base::BindOnce(&RuntimeServiceImpl::SendHeartbeat,
                                        weak_factory_.GetWeakPtr())));
}

void RuntimeServiceImpl::RecordMetrics(
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
      task_runner_, base::BindOnce(&RuntimeServiceImpl::OnMetricsRecorded,
                                   weak_factory_.GetWeakPtr(),
                                   std::move(record_complete_callback))));
}

void RuntimeServiceImpl::OnMetricsRecorded(
    CastRuntimeMetricsRecorderService::RecordCompleteCallback
        record_complete_callback,
    cast::utils::GrpcStatusOr<cast::metrics::RecordResponse> response_or) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response_or.ok()) {
    LOG(ERROR) << "Failed to record metrics: " << response_or.ToString();
  }

  std::move(record_complete_callback).Run();
}

void RuntimeServiceImpl::OnMetricsRecorderServiceStopped(
    cast::runtime::RuntimeServiceHandler::StopMetricsRecorder::Reactor*
        reactor) {
  DVLOG(2) << "MetricsRecorderService stopped";
  metrics_recorder_service_.reset();
  metrics_recorder_stub_.reset();
  action_recorder_.reset();

  reactor->Write(cast::runtime::StopMetricsRecorderResponse());
}

}  // namespace chromecast
