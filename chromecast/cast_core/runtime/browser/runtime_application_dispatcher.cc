// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_watcher.h"
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

}  // namespace

RuntimeApplicationDispatcher::RuntimeApplicationDispatcher(
    CastWebService* web_service,
    CastRuntimeMetricsRecorder::EventBuilderFactory* event_builder_factory,
    cast_streaming::NetworkContextGetter network_context_getter,
    media::VideoPlaneController* video_plane_controller,
    RuntimeApplicationWatcher* application_watcher)
    : web_service_(web_service),
      network_context_getter_(std::move(network_context_getter)),
      metrics_recorder_(event_builder_factory),
      video_plane_controller_(video_plane_controller),
      application_watcher_(application_watcher),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(web_service_);

  heartbeat_timer_.SetTaskRunner(task_runner_);
}

RuntimeApplicationDispatcher::~RuntimeApplicationDispatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

bool RuntimeApplicationDispatcher::Start(
    const std::string& runtime_id,
    const std::string& runtime_service_endpoint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!grpc_server_);

  LOG(INFO) << "Starting runtime service: runtime_id=" << runtime_id
            << ", endpoint=" << runtime_service_endpoint;

  grpc_server_.emplace();
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::LoadApplication>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationDispatcher::HandleLoadApplication,
                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::LaunchApplication>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationDispatcher::HandleLaunchApplication,
                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::StopApplication>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationDispatcher::HandleStopApplication,
                  weak_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<cast::runtime::RuntimeServiceHandler::Heartbeat>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(&RuntimeApplicationDispatcher::HandleHeartbeat,
                              weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::StartMetricsRecorder>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationDispatcher::HandleStartMetricsRecorder,
                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::runtime::RuntimeServiceHandler::StopMetricsRecorder>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationDispatcher::HandleStopMetricsRecorder,
                  weak_factory_.GetWeakPtr())));
  grpc_server_->Start(runtime_service_endpoint);

  LOG(INFO) << "Runtime service started";
  return true;
}

void RuntimeApplicationDispatcher::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetApp();

  if (heartbeat_reactor_) {
    heartbeat_timer_.Stop();
    heartbeat_reactor_->Write(grpc::Status::OK);
    heartbeat_reactor_ = nullptr;
  }

  if (grpc_server_) {
    grpc_server_->Stop();
    grpc_server_.reset();
  }

  if (metrics_recorder_service_) {
    metrics_recorder_service_->OnCloseSoon(base::DoNothing());
    metrics_recorder_service_.reset();
  }

  LOG(INFO) << "Runtime service stopped";
}

void RuntimeApplicationDispatcher::HandleLoadApplication(
    cast::runtime::LoadApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request.cast_session_id().empty()) {
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Application session ID is missing"));
    return;
  }
  if (!request.has_application_config()) {
    reactor->Write(
        grpc::Status(grpc::INVALID_ARGUMENT, "Application config is missing"));
    return;
  }

  const std::string& app_id = request.application_config().app_id();
  if (openscreen::cast::IsCastStreamingReceiverAppId(app_id)) {
    DCHECK(video_plane_controller_);
    // Deliberately copy |network_context_getter_|.
    app_ = std::make_unique<StreamingRuntimeApplication>(
        request.cast_session_id(), request.application_config(), web_service_,
        task_runner_, network_context_getter_, video_plane_controller_);
  } else {
    app_ = std::make_unique<WebRuntimeApplication>(request.cast_session_id(),
                                                   request.application_config(),
                                                   web_service_, task_runner_);
  }

  if (application_watcher_) {
    application_watcher_->OnRuntimeApplicationChanged(app_.get());
  }

  app_->Load(
      std::move(request),
      base::BindPostTask(
          task_runner_,
          base::BindOnce(&RuntimeApplicationDispatcher::OnApplicationLoaded,
                         weak_factory_.GetWeakPtr(), std::move(reactor))));
}

void RuntimeApplicationDispatcher::HandleLaunchApplication(
    cast::runtime::LaunchApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  app_->Launch(
      std::move(request),
      base::BindPostTask(
          task_runner_,
          base::BindOnce(&RuntimeApplicationDispatcher::OnApplicationLaunched,
                         weak_factory_.GetWeakPtr(), std::move(reactor))));
}

void RuntimeApplicationDispatcher::HandleStopApplication(
    cast::runtime::StopApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::StopApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!app_) {
    reactor->Write(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                                "No application is running"));
    return;
  }

  // Reset the app only after the response is constructed.
  cast::runtime::StopApplicationResponse response;
  response.set_app_id(app_->GetAppConfig().app_id());
  response.set_cast_session_id(app_->GetCastSessionId());

  ResetApp();
  reactor->Write(std::move(response));
}

void RuntimeApplicationDispatcher::HandleHeartbeat(
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
      base::BindRepeating(&RuntimeApplicationDispatcher::OnHeartbeatSent,
                          weak_factory_.GetWeakPtr())));
  DVLOG(2) << "Starting heartbeat ticking with period: " << heartbeat_period_
           << " seconds";

  SendHeartbeat();
}

void RuntimeApplicationDispatcher::HandleStartMetricsRecorder(
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
      base::BindRepeating(&RuntimeApplicationDispatcher::RecordMetrics,
                          weak_factory_.GetWeakPtr()),
      kDefaultMetricsReportInterval);
  DVLOG(2) << "MetricsRecorderService connected: endpoint="
           << request.metrics_recorder_service_info().grpc_endpoint();
  reactor->Write(cast::runtime::StartMetricsRecorderResponse());
}

void RuntimeApplicationDispatcher::HandleStopMetricsRecorder(
    cast::runtime::StopMetricsRecorderRequest request,
    cast::runtime::RuntimeServiceHandler::StopMetricsRecorder::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics_recorder_service_->OnCloseSoon(base::BindOnce(
      &RuntimeApplicationDispatcher::OnMetricsRecorderServiceStopped,
      weak_factory_.GetWeakPtr(), std::move(reactor)));
}

void RuntimeApplicationDispatcher::OnApplicationLoaded(
    cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor,
    grpc::Status status) {
  if (!status.ok()) {
    LOG(ERROR) << "Failed to load application: " << *app_
               << ", status=" << cast::utils::GrpcStatusToString(status);
    ResetApp();
    reactor->Write(status);
    return;
  }

  LOG(INFO) << "Application loaded: " << *app_;
  cast::runtime::LoadApplicationResponse response;
  response.mutable_message_port_info();
  reactor->Write(std::move(response));
}

void RuntimeApplicationDispatcher::OnApplicationLaunched(
    cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor,
    grpc::Status status) {
  if (!status.ok()) {
    LOG(ERROR) << "Failed to launch application: " << *app_
               << ", status=" << cast::utils::GrpcStatusToString(status);
    ResetApp();
    reactor->Write(status);
    return;
  }

  LOG(INFO) << "Application launched: " << *app_;
  reactor->Write(cast::runtime::LaunchApplicationResponse());
}

void RuntimeApplicationDispatcher::SendHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(heartbeat_reactor_);
  DVLOG(2) << "Sending heartbeat";
  heartbeat_reactor_->Write(cast::runtime::HeartbeatResponse());
}

void RuntimeApplicationDispatcher::OnHeartbeatSent(
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
          base::BindOnce(&RuntimeApplicationDispatcher::SendHeartbeat,
                         weak_factory_.GetWeakPtr())));
}

void RuntimeApplicationDispatcher::RecordMetrics(
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
      base::BindOnce(&RuntimeApplicationDispatcher::OnMetricsRecorded,
                     weak_factory_.GetWeakPtr(),
                     std::move(record_complete_callback))));
}

void RuntimeApplicationDispatcher::OnMetricsRecorded(
    CastRuntimeMetricsRecorderService::RecordCompleteCallback
        record_complete_callback,
    cast::utils::GrpcStatusOr<cast::metrics::RecordResponse> response_or) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response_or.ok()) {
    LOG(ERROR) << "Failed to record metrics: " << response_or.ToString();
  }

  std::move(record_complete_callback).Run();
}

void RuntimeApplicationDispatcher::OnMetricsRecorderServiceStopped(
    cast::runtime::RuntimeServiceHandler::StopMetricsRecorder::Reactor*
        reactor) {
  DVLOG(2) << "MetricsRecorderService stopped";
  metrics_recorder_service_.reset();
  reactor->Write(cast::runtime::StopMetricsRecorderResponse());
}

void RuntimeApplicationDispatcher::ResetApp() {
  app_.reset();
  if (application_watcher_) {
    application_watcher_->OnRuntimeApplicationChanged(nullptr);
  }
}

const std::string& RuntimeApplicationDispatcher::GetCastMediaServiceEndpoint()
    const {
  return app_->GetCastMediaServiceEndpoint();
}

CastWebService* RuntimeApplicationDispatcher::GetCastWebService() const {
  return web_service_;
}

RuntimeApplication* RuntimeApplicationDispatcher::GetRuntimeApplication() {
  return app_.get();
}

}  // namespace chromecast
