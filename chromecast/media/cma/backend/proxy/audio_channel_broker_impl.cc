// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/audio_channel_broker_impl.h"

#include <utility>

#include "chromecast/media/cma/backend/proxy/cast_runtime_audio_channel_endpoint_manager.h"
#include "chromecast/public/task_runner.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"

namespace chromecast {
namespace media {
namespace {

constexpr int64_t kMsDelayBetweenPushBufferPollingChecks = 10;

std::unique_ptr<cast::media::CastAudioChannelService::StubInterface>
GetRemoteStub() {
  auto* endpoint_manager = CastRuntimeAudioChannelEndpointManager::Get();
  DCHECK(endpoint_manager);

  std::string endpoint = endpoint_manager->GetAudioChannelEndpoint();
  DCHECK(!endpoint.empty());

  auto channel =
      grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  return cast::media::CastAudioChannelService::NewStub(channel);
}

}  // namespace

// static
std::unique_ptr<CastRuntimeAudioChannelBroker>
CastRuntimeAudioChannelBroker::Create(TaskRunner* task_runner,
                                      Handler* handler) {
  return std::make_unique<AudioChannelBrokerImpl>(task_runner, handler);
}

AudioChannelBrokerImpl::LazyStubFactory::LazyStubFactory() = default;

AudioChannelBrokerImpl::LazyStubFactory::~LazyStubFactory() = default;

AudioChannelBrokerImpl::LazyStubFactory::Stub*
AudioChannelBrokerImpl::LazyStubFactory::GetStub() {
  if (!stub_) {
    stub_ = GetRemoteStub();
  }

  return stub_.get();
}

AudioChannelBrokerImpl::AudioChannelBrokerImpl(TaskRunner* task_runner,
                                               Handler* handler)
    : handler_(handler), task_runner_(task_runner), weak_factory_(this) {
  DCHECK(task_runner_);
  DCHECK(handler_);
}

AudioChannelBrokerImpl::~AudioChannelBrokerImpl() = default;

void AudioChannelBrokerImpl::InitializeAsync(
    const std::string& cast_session_id,
    CastAudioDecoderMode decoder_mode) {
  auto request = InitializeCall::CreateRequest(
      this, &GrpcStub::async_interface::Initialize,
      &AudioChannelBrokerImpl::OnInitialize);
  request.parameters().set_cast_session_id(cast_session_id);
  request.parameters().set_mode(decoder_mode);

  request.CallAsync();
}

void AudioChannelBrokerImpl::SetVolumeAsync(float multiplier) {
  auto request =
      SetVolumeCall::CreateRequest(this, &GrpcStub::async_interface::SetVolume,
                                   &AudioChannelBrokerImpl::OnSetVolume);
  request.parameters().set_multiplier(multiplier);

  request.CallAsync();
}

void AudioChannelBrokerImpl::SetPlaybackAsync(double playback_rate) {
  auto request = SetPlaybackCall::CreateRequest(
      this, &GrpcStub::async_interface::SetPlaybackRate,
      &AudioChannelBrokerImpl::OnSetPlayback);
  request.parameters().set_rate(playback_rate);

  request.CallAsync();
}

void AudioChannelBrokerImpl::GetMediaTimeAsync() {
  auto request = GetMediaTimeCall::CreateRequest(
      this, &GrpcStub::async_interface::GetMediaTime,
      &AudioChannelBrokerImpl::OnGetMediaTime);

  request.CallAsync();
}

AudioChannelBrokerImpl::StateChangeCall::Request
AudioChannelBrokerImpl::CreateStateChangeCallRequest() {
  return StateChangeCall::CreateRequest(this,
                                        &GrpcStub::async_interface::StateChange,
                                        &AudioChannelBrokerImpl::OnStateChange);
}

void AudioChannelBrokerImpl::StartAsync(int64_t pts_micros,
                                        TimestampInfo timestamp_info) {
  auto request = CreateStateChangeCallRequest();
  request.parameters().mutable_start()->set_pts_micros(pts_micros);
  *request.parameters().mutable_start()->mutable_timestamp_info() =
      timestamp_info;
  request.CallAsync();
}

void AudioChannelBrokerImpl::StopAsync() {
  auto request = CreateStateChangeCallRequest();
  request.parameters().set_allocated_stop(new cast::media::StopRequest());
  request.CallAsync();
}

void AudioChannelBrokerImpl::PauseAsync() {
  auto request = CreateStateChangeCallRequest();
  request.parameters().set_allocated_pause(new cast::media::PauseRequest());
  request.CallAsync();
}

void AudioChannelBrokerImpl::ResumeAsync(TimestampInfo timestamp_info) {
  auto request = CreateStateChangeCallRequest();
  *request.parameters().mutable_resume()->mutable_resume_timestamp_info() =
      timestamp_info;
  request.CallAsync();
}

void AudioChannelBrokerImpl::UpdateTimestampAsync(
    TimestampInfo timestamp_info) {
  auto request = CreateStateChangeCallRequest();
  *request.parameters().mutable_timestamp_update()->mutable_timestamp_info() =
      timestamp_info;
  request.CallAsync();
}

void AudioChannelBrokerImpl::TryPushBuffer() {
  if (handler_->HasBufferedData()) {
    auto request = handler_->GetBufferedData();
    if (request.has_value()) {
      auto request_call = std::make_unique<cast::media::PushBufferRequest>(
          std::move(request.value()));
      DCHECK(request_call.get());

      auto push_buffer_request = PushBufferCall::CreateRequest(
          this, &GrpcStub::async_interface::PushBuffer,
          &AudioChannelBrokerImpl::OnPushBuffer);
      push_buffer_request.parameters().CopyFrom(*request_call);

      push_buffer_request.CallAsync();
      return;
    }
  }

  auto* task = new TaskRunner::CallbackTask<base::OnceClosure>(base::BindOnce(
      &AudioChannelBrokerImpl::TryPushBuffer, weak_factory_.GetWeakPtr()));
  task_runner_->PostTask(task, kMsDelayBetweenPushBufferPollingChecks);
}

void AudioChannelBrokerImpl::OnGetMediaTime(
    std::unique_ptr<GetMediaTimeCall::Response> request,
    CastRuntimeAudioChannelBroker::StatusCode status_code) {
  handler_->HandleGetMediaTimeResponse(request->response().media_time(),
                                       status_code);
}

void AudioChannelBrokerImpl::OnPushBuffer(
    std::unique_ptr<PushBufferCall::Response> request,
    CastRuntimeAudioChannelBroker::StatusCode status_code) {
  handler_->HandlePushBufferResponse(request->response().decoded_bytes(),
                                     status_code);
  TryPushBuffer();
}

void AudioChannelBrokerImpl::OnSetPlayback(
    std::unique_ptr<SetPlaybackCall::Response> request,
    CastRuntimeAudioChannelBroker::StatusCode status_code) {
  handler_->HandleSetPlaybackResponse(status_code);
}

void AudioChannelBrokerImpl::OnSetVolume(
    std::unique_ptr<SetVolumeCall::Response> request,
    CastRuntimeAudioChannelBroker::StatusCode status_code) {
  handler_->HandleSetVolumeResponse(status_code);
}

void AudioChannelBrokerImpl::OnStateChange(
    std::unique_ptr<StateChangeCall::Response> request,
    CastRuntimeAudioChannelBroker::StatusCode status_code) {
  handler_->HandleStateChangeResponse(request->response().state(), status_code);
}

void AudioChannelBrokerImpl::OnInitialize(
    std::unique_ptr<InitializeCall::Response> request,
    CastRuntimeAudioChannelBroker::StatusCode status_code) {
  handler_->HandleInitializeResponse(status_code);
}

}  // namespace media
}  // namespace chromecast
