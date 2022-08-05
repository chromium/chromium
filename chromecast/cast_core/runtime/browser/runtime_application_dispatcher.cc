// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_service.h"
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
    media::VideoPlaneController* video_plane_controller)
    : web_service_(web_service),
      network_context_getter_(std::move(network_context_getter)),
      metrics_recorder_(event_builder_factory),
      video_plane_controller_(video_plane_controller),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(web_service_);

  heartbeat_timer_.SetTaskRunner(task_runner_);
}

RuntimeApplicationDispatcher::~RuntimeApplicationDispatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

void RuntimeApplicationDispatcher::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void RuntimeApplicationDispatcher::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
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
  loaded_apps_.clear();

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

void RuntimeApplicationDispatcher::HandleLoadApplication(
    cast::runtime::LoadApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request.cast_session_id().empty()) {
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Application session ID is missing"));
    return;
  }
  if (loaded_apps_.contains(request.cast_session_id())) {
    reactor->Write(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "Application already exists"));
    return;
  }
  if (!request.has_application_config()) {
    reactor->Write(
        grpc::Status(grpc::INVALID_ARGUMENT, "Application config is missing"));
    return;
  }

  std::unique_ptr<RuntimeApplication> app;
  if (openscreen::cast::IsCastStreamingReceiverAppId(
          request.application_config().app_id())) {
    DCHECK(video_plane_controller_);
    // Deliberately copy |network_context_getter_|.
    app = std::make_unique<StreamingRuntimeApplication>(
        request.cast_session_id(), request.application_config(), web_service_,
        task_runner_, network_context_getter_, video_plane_controller_);
  } else {
    app = std::make_unique<WebRuntimeApplication>(request.cast_session_id(),
                                                  request.application_config(),
                                                  web_service_, task_runner_);
  }

  // TODO(b/232140331): Call this only when foreground app changes.
  base::ranges::for_each(observers_, [app = app.get()](auto& observer) {
    observer.OnForegroundApplicationChanged(app);
  });

  // Need to cache session_id as |request| object is moved.
  std::string session_id = request.cast_session_id();
  app->Load(
      std::move(request),
      base::BindPostTask(
          task_runner_,
          base::BindOnce(&RuntimeApplicationDispatcher::OnApplicationLoaded,
                         weak_factory_.GetWeakPtr(), session_id,
                         std::move(reactor))));

  loaded_apps_.emplace(std::move(session_id), std::move(app));
}

void RuntimeApplicationDispatcher::HandleLaunchApplication(
    cast::runtime::LaunchApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Need to cache session_id as |request| object is moved.
  std::string session_id = request.cast_session_id();
  auto* app = GetApp(session_id);
  if (!app) {
    LOG(ERROR) << "Application doesn't exist anymore: session_id" << session_id;
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Application not found"));
    return;
  }

  app->Launch(
      std::move(request),
      base::BindPostTask(
          task_runner_,
          base::BindOnce(&RuntimeApplicationDispatcher::OnApplicationLaunching,
                         weak_factory_.GetWeakPtr(), std::move(session_id),
                         std::move(reactor))));
}

void RuntimeApplicationDispatcher::HandleStopApplication(
    cast::runtime::StopApplicationRequest request,
    cast::runtime::RuntimeServiceHandler::StopApplication::Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* app = GetApp(request.cast_session_id());
  if (!app) {
    LOG(ERROR) << "Application doesn't exist anymore: session_id"
               << request.cast_session_id();
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Application not found"));
    return;
  }

  // Reset the app only after the response is constructed.
  cast::runtime::StopApplicationResponse response;
  response.set_app_id(app->GetAppConfig().app_id());
  response.set_cast_session_id(app->GetCastSessionId());
  reactor->Write(std::move(response));

  ResetApp(request.cast_session_id());
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
    std::string session_id,
    cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor,
    grpc::Status status) {
  auto* app = GetApp(session_id);
  if (!app) {
    LOG(ERROR) << "Application doesn't exist anymore: session_id" << session_id;
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Application not found"));
    return;
  }

  if (!status.ok()) {
    LOG(ERROR) << "Failed to load application: " << *app
               << ", status=" << cast::utils::GrpcStatusToString(status);
    ResetApp(session_id);
    reactor->Write(status);
    return;
  }

  LOG(INFO) << "Application loaded: " << *app;
  cast::runtime::LoadApplicationResponse response;
  response.mutable_message_port_info();
  reactor->Write(std::move(response));
}

void RuntimeApplicationDispatcher::OnApplicationLaunching(
    std::string session_id,
    cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor,
    grpc::Status status) {
  auto* app = GetApp(session_id);
  if (!app) {
    LOG(ERROR) << "Application doesn't exist anymore: session_id" << session_id;
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Application not found"));
    return;
  }

  if (!status.ok()) {
    LOG(ERROR) << "Failed to launch application: " << *app
               << ", status=" << cast::utils::GrpcStatusToString(status);
    ResetApp(session_id);
    reactor->Write(status);
    return;
  }

  LOG(INFO) << "Application is starting: " << *app;
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

RuntimeApplication* RuntimeApplicationDispatcher::GetApp(
    const std::string& session_id) const {
  auto iter = loaded_apps_.find(session_id);
  if (iter == loaded_apps_.end()) {
    return nullptr;
  }
  return iter->second.get();
}

void RuntimeApplicationDispatcher::ResetApp(const std::string& session_id) {
  auto iter = loaded_apps_.find(session_id);
  DCHECK(iter != loaded_apps_.end());
  loaded_apps_.erase(iter);

  // TODO(b/232140331): Call this only when foreground app changes.
  base::ranges::for_each(observers_, [](auto& observer) {
    observer.OnForegroundApplicationChanged(nullptr);
  });
}

const std::string& RuntimeApplicationDispatcher::GetCastMediaServiceEndpoint()
    const {
  // TODO(b/232140331): Call this only when foreground app changes.
  DCHECK(!loaded_apps_.empty());
  return loaded_apps_.begin()->second->GetCastMediaServiceEndpoint();
}

}  // namespace chromecast
