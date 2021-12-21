// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher.h"

#include "base/notreached.h"
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

base::TimeDelta kDefaultMetricsReportInterval = base::Seconds(60);

}  // namespace

RuntimeApplicationDispatcher::AsyncMetricsRecord::AsyncMetricsRecord(
    const cast::metrics::RecordRequest& request,
    cast::metrics::MetricsRecorderService::Stub* metrics_recorder_stub,
    grpc::CompletionQueue* cq,
    base::WeakPtr<RuntimeApplicationDispatcher> runtime_service)
    : runtime_service_(runtime_service) {
  response_reader_ =
      metrics_recorder_stub->PrepareAsyncRecord(&context_, request, cq);
  response_reader_->StartCall();
  response_reader_->Finish(&response_, &status_, static_cast<GRPC*>(this));
}

RuntimeApplicationDispatcher::AsyncMetricsRecord::~AsyncMetricsRecord() =
    default;

void RuntimeApplicationDispatcher::AsyncMetricsRecord::StepGRPC(
    grpc::Status status) {
  if (runtime_service_) {
    runtime_service_->OnRecordComplete();
  }
  delete this;
}

RuntimeApplicationDispatcher::RuntimeApplicationDispatcher(
    CastWebService* web_service,
    CastRuntimeMetricsRecorder::EventBuilderFactory* event_builder_factory,
    cast_streaming::NetworkContextGetter network_context_getter)
    : GrpcServer(base::SequencedTaskRunnerHandle::Get()),
      web_service_(web_service),
      network_context_getter_(std::move(network_context_getter)),
      metrics_recorder_(event_builder_factory) {}

RuntimeApplicationDispatcher::~RuntimeApplicationDispatcher() {
  Stop();
}

bool RuntimeApplicationDispatcher::Start(
    const std::string& runtime_id,
    const std::string& runtime_service_path) {
  runtime_id_ = runtime_id;
  runtime_service_path_ = runtime_service_path;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(runtime_service_path_,
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&grpc_runtime_service_);
  SetCompletionQueue(builder.AddCompletionQueue());
  SetServer(builder.BuildAndStart());
  if (!grpc_server_) {
    LOG(ERROR) << "Failed to start server on path: " << runtime_service_path_;
    return false;
  }

  StartRuntimeServiceMethods(&grpc_runtime_service_, weak_factory_.GetWeakPtr(),
                             grpc_cq_, &is_shutdown_);
  GrpcServer::Start();

  return true;
}

void RuntimeApplicationDispatcher::Stop() {
  app_.reset();

  if (heartbeat_) {
    heartbeat_->Finish(grpc::Status::CANCELLED);
    heartbeat_ = nullptr;
  }
  if (metrics_recorder_client_) {
    metrics_recorder_client_->OnCloseSoon(base::DoNothing());
  }
  GrpcServer::Stop();
}

void RuntimeApplicationDispatcher::Record(
    const cast::metrics::RecordRequest& request) {
  if (!metrics_recorder_stub_) {
    return;
  }
  if (!grpc_cq_) {
    return;
  }
  new AsyncMetricsRecord(request, metrics_recorder_stub_.get(), grpc_cq_,
                         weak_factory_.GetWeakPtr());
}

void RuntimeApplicationDispatcher::LoadApplication(
    const cast::runtime::LoadApplicationRequest& request,
    cast::runtime::LoadApplicationResponse* response,
    GrpcMethod* callback) {
  const std::string& app_id = request.application_config().app_id();

  if (openscreen::cast::IsCastStreamingReceiverAppId(app_id)) {
    // Deliberately copy |network_context_getter_|.
    app_ = std::make_unique<StreamingRuntimeApplication>(
        web_service_, task_runner_, network_context_getter_);
  } else {
    app_ = std::make_unique<WebRuntimeApplication>(web_service_, task_runner_);
  }
  if (!app_->Load(request)) {
    app_.reset();
    std::stringstream err_stream;
    err_stream
        << "failed to load RuntimeApplication (session id: "
        << request.cast_session_id() << ", app id: "
        << (request.has_application_config()
                ? request.application_config().app_id()
                : "NONE")
        << ", grpc endpoint: "
        << (request.has_runtime_application_service_info()
                ? request.runtime_application_service_info().grpc_endpoint()
                : "NONE")
        << ")";
    callback->StepGRPC(grpc::Status(grpc::INTERNAL, err_stream.str()));
    return;
  }
  cast::runtime::MessagePortInfo info;
  *response->mutable_message_port_info() = info;
  DCHECK(response->has_message_port_info());
  callback->StepGRPC(grpc::Status::OK);
}

void RuntimeApplicationDispatcher::LaunchApplication(
    const cast::runtime::LaunchApplicationRequest& request,
    cast::runtime::LaunchApplicationResponse* response,
    GrpcMethod* callback) {
  if (!app_->Launch(request)) {
    std::stringstream err_stream;
    err_stream << "failed to launch RuntimeApplication (session id: "
               << app_->cast_session_id()
               << ", app id: " << app_->app_config().app_id()
               << ", cast media service endpoint: "
               << (request.has_cast_media_service_info()
                       ? request.cast_media_service_info().grpc_endpoint()
                       : "NONE")
               << ")";
    app_.reset();
    callback->StepGRPC(grpc::Status(grpc::INTERNAL, err_stream.str()));
    return;
  }
  callback->StepGRPC(grpc::Status::OK);
}

void RuntimeApplicationDispatcher::StopApplication(
    const cast::runtime::StopApplicationRequest& request,
    cast::runtime::StopApplicationResponse* response,
    GrpcMethod* callback) {
  response->set_app_id(app_->app_config().app_id());
  response->set_cast_session_id(app_->cast_session_id());
  app_.reset();
  callback->StepGRPC(grpc::Status::OK);
}

void RuntimeApplicationDispatcher::Heartbeat(
    const cast::runtime::HeartbeatRequest& request,
    HeartbeatMethod* heartbeat) {
  if (heartbeat_) {
    heartbeat->Finish(grpc::Status::CANCELLED);
    return;
  }
  if (!request.has_heartbeat_period() ||
      request.heartbeat_period().seconds() <= 0) {
    heartbeat->Finish(grpc::Status::CANCELLED);
    return;
  }
  heartbeat_period_ = request.heartbeat_period().seconds();
  heartbeat_ = heartbeat;
  DVLOG(2) << "Starting heartbeat ticking with period: " << heartbeat_period_
           << " seconds";
  SendHeartbeat();
}

void RuntimeApplicationDispatcher::StartMetricsRecorder(
    const cast::runtime::StartMetricsRecorderRequest& request,
    cast::runtime::StartMetricsRecorderResponse* response,
    GrpcMethod* callback) {
  auto metrics_channel = grpc::CreateChannel(
      request.metrics_recorder_service_info().grpc_endpoint(),
      grpc::InsecureChannelCredentials());
  if (!metrics_channel) {
    DLOG(WARNING) << "Failed to open a channel to MetricsRecorderService.";
    callback->StepGRPC(
        grpc::Status(grpc::StatusCode::INTERNAL,
                     "Failed to open channel to MetricsRecorderService"));
    return;
  }

  metrics_recorder_stub_ = cast::metrics::MetricsRecorderService::NewStub(
      std::move(metrics_channel));
  metrics_recorder_service_ =
      std::make_unique<CastRuntimeMetricsRecorderService>(
          &metrics_recorder_, &action_recorder_, this,
          kDefaultMetricsReportInterval);
  DVLOG(2) << "MetricsRecorderService started";
  callback->StepGRPC(grpc::Status::OK);
}

void RuntimeApplicationDispatcher::StopMetricsRecorder(
    const cast::runtime::StopMetricsRecorderRequest& request,
    cast::runtime::StopMetricsRecorderResponse* response,
    GrpcMethod* callback) {
  metrics_recorder_client_->OnCloseSoon(base::BindOnce(
      &RuntimeApplicationDispatcher::OnMetricsRecorderServiceStopped,
      base::Unretained(this), base::Unretained(callback)));
}

void RuntimeApplicationDispatcher::SendHeartbeat() {
  DVLOG(2) << "Sending heartbeat";
  if (heartbeat_) {
    heartbeat_->Tick();
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RuntimeApplicationDispatcher::SendHeartbeat,
                       weak_factory_.GetWeakPtr()),
        base::Seconds(heartbeat_period_));
  }
}

void RuntimeApplicationDispatcher::OnRecordComplete() {
  metrics_recorder_client_->OnRecordComplete();
}

void RuntimeApplicationDispatcher::OnMetricsRecorderServiceStopped(
    GrpcMethod* callback) {
  DVLOG(2) << "MetricsRecorderService stopped";
  metrics_recorder_service_.reset();
  callback->StepGRPC(grpc::Status::OK);
}

const std::string&
RuntimeApplicationDispatcher::GetCastMediaServiceGrpcEndpoint() const {
  return app_->cast_media_service_grpc_endpoint();
}

CastWebService* RuntimeApplicationDispatcher::GetCastWebService() const {
  return web_service_;
}

RuntimeApplication* RuntimeApplicationDispatcher::GetRuntimeApplication() {
  return app_.get();
}

}  // namespace chromecast
