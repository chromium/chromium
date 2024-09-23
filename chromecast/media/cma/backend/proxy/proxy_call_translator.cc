// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/proxy_call_translator.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/proxy/push_buffer_pending_handler.h"
#include "chromecast/media/cma/backend/proxy/push_buffer_queue.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/task_runner.h"
#include "third_party/cast_core/public/src/proto/runtime/cast_audio_channel_service.grpc.pb.h"
#include "third_party/protobuf/src/google/protobuf/util/time_util.h"

namespace chromecast {
namespace media {
namespace {

CmaProxyHandler::PipelineState ToClientTypes(
    CastRuntimeAudioChannelBroker::Handler::PipelineState state) {
  switch (state) {
    case CastRuntimeAudioChannelBroker::Handler::PipelineState::
        PIPELINE_STATE_UNINITIALIZED:
      return CmaProxyHandler::PipelineState::kUninitialized;
    case CastRuntimeAudioChannelBroker::Handler::PipelineState::
        PIPELINE_STATE_STOPPED:
      return CmaProxyHandler::PipelineState::kStopped;
    case CastRuntimeAudioChannelBroker::Handler::PipelineState::
        PIPELINE_STATE_PLAYING:
      return CmaProxyHandler::PipelineState::kPlaying;
    case CastRuntimeAudioChannelBroker::Handler::PipelineState::
        PIPELINE_STATE_PAUSED:
      return CmaProxyHandler::PipelineState::kPaused;
    default:
      NOTREACHED();
  }
}

cast::media::AudioConfiguration_AudioCodec ToGrpcTypes(AudioCodec codec) {
  switch (codec) {
    case kAudioCodecUnknown:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_UNKNOWN;
    case kCodecAAC:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_AAC;
    case kCodecMP3:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_MP3;
    case kCodecPCM:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_PCM;
    case kCodecPCM_S16BE:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_PCM_S16BE;
    case kCodecVorbis:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_VORBIS;
    case kCodecOpus:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_OPUS;
    case kCodecEAC3:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_EAC3;
    case kCodecAC3:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_AC3;
    case kCodecDTS:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_DTS;
    case kCodecFLAC:
      return cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_FLAC;
    case kCodecMpegHAudio:
      return cast::media::
          AudioConfiguration_AudioCodec_AUDIO_CODEC_MPEG_H_AUDIO;
    default:
      NOTREACHED();
  }
}

cast::media::AudioConfiguration_ChannelLayout ToGrpcTypes(
    ChannelLayout channel_layout) {
  switch (channel_layout) {
    case ChannelLayout::UNSUPPORTED:
      return cast::media::
          AudioConfiguration_ChannelLayout_CHANNEL_LAYOUT_UNSUPPORTED;
    case ChannelLayout::MONO:
      return cast::media::AudioConfiguration_ChannelLayout_CHANNEL_LAYOUT_MONO;
    case ChannelLayout::STEREO:
      return cast::media::
          AudioConfiguration_ChannelLayout_CHANNEL_LAYOUT_STEREO;
    case ChannelLayout::SURROUND_5_1:
      return cast::media::
          AudioConfiguration_ChannelLayout_CHANNEL_LAYOUT_SURROUND_5_1;
    case ChannelLayout::BITSTREAM:
      return cast::media::
          AudioConfiguration_ChannelLayout_CHANNEL_LAYOUT_BITSTREAM;
    case ChannelLayout::DISCRETE:
      return cast::media::
          AudioConfiguration_ChannelLayout_CHANNEL_LAYOUT_DISCRETE;
    default:
      NOTREACHED();
  }
}

cast::media::AudioConfiguration_SampleFormat ToGrpcTypes(
    SampleFormat sample_format) {
  switch (sample_format) {
    case kUnknownSampleFormat:
      return cast::media::AudioConfiguration_SampleFormat_SAMPLE_FORMAT_UNKNOWN;
    case kSampleFormatU8:
      return cast::media::AudioConfiguration_SampleFormat_SAMPLE_FORMAT_U8;
    case kSampleFormatS16:
      return cast::media::AudioConfiguration_SampleFormat_SAMPLE_FORMAT_S16;
    case kSampleFormatS32:
      return cast::media::AudioConfiguration_SampleFormat_SAMPLE_FORMAT_S32;
    case kSampleFormatF32:
      return cast::media::AudioConfiguration_SampleFormat_SAMPLE_FORMAT_F32;
    case kSampleFormatPlanarS16:
      return cast::media::
          AudioConfiguration_SampleFormat_SAMPLE_FORMAT_PLANAR_S16;
    case kSampleFormatPlanarF32:
      return cast::media::
          AudioConfiguration_SampleFormat_SAMPLE_FORMAT_PLANAR_F32;
    case kSampleFormatPlanarS32:
      return cast::media::
          AudioConfiguration_SampleFormat_SAMPLE_FORMAT_PLANAR_S32;
    case kSampleFormatS24:
      return cast::media::AudioConfiguration_SampleFormat_SAMPLE_FORMAT_S24;
    default:
      NOTREACHED();
  }
}

cast::media::CastAudioDecoderMode ToGrpcTypes(
    CmaProxyHandler::AudioDecoderOperationMode operation_mode) {
  switch (operation_mode) {
    case CmaProxyHandler::AudioDecoderOperationMode::kAll:
      return cast::media::CAST_AUDIO_DECODER_MODE_ALL;
    case CmaProxyHandler::AudioDecoderOperationMode::kMultiroomOnly:
      return cast::media::CAST_AUDIO_DECODER_MODE_MULTIROOM_ONLY;
    case CmaProxyHandler::AudioDecoderOperationMode::kAudioOnly:
      return cast::media::CAST_AUDIO_DECODER_MODE_AUDIO_ONLY;
  }
}

CastRuntimeAudioChannelBroker::Handler::PushBufferRequest ToGrpcTypes(
    scoped_refptr<DecoderBufferBase> buffer,
    BufferIdManager::BufferId buffer_id) {
  auto* decode_buffer = new cast::media::AudioDecoderBuffer;

  decode_buffer->set_id(buffer_id);
  decode_buffer->set_end_of_stream(buffer->end_of_stream());
  if (!buffer->end_of_stream()) {
    decode_buffer->set_pts_micros(buffer->timestamp());
    decode_buffer->set_data(buffer->data(), buffer->data_size());
  }

  CastRuntimeAudioChannelBroker::Handler::PushBufferRequest request;

  // NOTE: This transfers ownership of |decode_buffer| to |request|.
  request.set_allocated_buffer(decode_buffer);

  return request;
}

CastRuntimeAudioChannelBroker::Handler::PushBufferRequest ToGrpcTypes(
    const AudioConfig& audio_config) {
  auto* audio_config_internal = new cast::media::AudioConfiguration;
  audio_config_internal->set_codec(ToGrpcTypes(audio_config.codec));
  audio_config_internal->set_channel_layout(
      ToGrpcTypes(audio_config.channel_layout));
  audio_config_internal->set_sample_format(
      ToGrpcTypes(audio_config.sample_format));
  audio_config_internal->set_bytes_per_channel(audio_config.bytes_per_channel);
  audio_config_internal->set_channel_number(audio_config.channel_number);
  audio_config_internal->set_samples_per_second(
      audio_config.samples_per_second);

  // NOTE: This copies the data.
  audio_config_internal->set_extra_data(audio_config.extra_data.data(),
                                        audio_config.extra_data.size());

  CastRuntimeAudioChannelBroker::Handler::PushBufferRequest request;

  // NOTE: This transfers ownership of |audio_config_internal| to |request|.
  request.set_allocated_audio_config(audio_config_internal);

  return request;
}

CastRuntimeAudioChannelBroker::TimestampInfo ToGrpcTypes(
    const BufferIdManager::TargetBufferInfo& target_buffer) {
  CastRuntimeAudioChannelBroker::TimestampInfo ts_info;
  ts_info.set_buffer_id(target_buffer.buffer_id);
  cast::common::Duration cc_duration;
  cc_duration.set_seconds(target_buffer.timestamp_micros /
                          base::Time::kMicrosecondsPerSecond);
  cc_duration.set_nanoseconds(
      (target_buffer.timestamp_micros % base::Time::kMicrosecondsPerSecond) *
      base::Time::kNanosecondsPerMicrosecond);
  *ts_info.mutable_system_timestamp() = std::move(cc_duration);
  return ts_info;
}

}  // namespace

// static
std::unique_ptr<CmaProxyHandler> CmaProxyHandler::Create(
    TaskRunner* task_runner,
    Client* client,
    AudioChannelPushBufferHandler::Client* push_buffer_client) {
  return std::make_unique<ProxyCallTranslator>(task_runner, client,
                                               push_buffer_client);
}

ProxyCallTranslator::ProxyCallTranslator(
    TaskRunner* client_task_runner,
    CmaProxyHandler::Client* client,
    AudioChannelPushBufferHandler::Client* push_buffer_client)
    : ProxyCallTranslator(
          client_task_runner,
          client,
          push_buffer_client,
          CastRuntimeAudioChannelBroker::Create(client_task_runner, this)) {}

ProxyCallTranslator::ProxyCallTranslator(
    TaskRunner* client_task_runner,
    CmaProxyHandler::Client* client,
    AudioChannelPushBufferHandler::Client* push_buffer_client,
    std::unique_ptr<CastRuntimeAudioChannelBroker> decoder_channel)
    : decoder_channel_(std::move(decoder_channel)),
      client_task_runner_(client_task_runner),
      client_(client),
      push_buffer_handler_(client_task_runner, push_buffer_client),
      weak_factory_(this) {
  DCHECK(decoder_channel_.get());
  DCHECK(client_task_runner_);
  DCHECK(client_);
}

ProxyCallTranslator::~ProxyCallTranslator() = default;

void ProxyCallTranslator::Initialize(
    const std::string& cast_session_id,
    CmaProxyHandler::AudioDecoderOperationMode decoder_mode) {
  decoder_channel_->InitializeAsync(cast_session_id, ToGrpcTypes(decoder_mode));
}

void ProxyCallTranslator::Start(
    int64_t start_pts,
    const BufferIdManager::TargetBufferInfo& target_buffer) {
  decoder_channel_->StartAsync(start_pts, ToGrpcTypes(target_buffer));
}

void ProxyCallTranslator::Stop() {
  decoder_channel_->StopAsync();
}

void ProxyCallTranslator::Pause() {
  decoder_channel_->PauseAsync();
}

void ProxyCallTranslator::Resume(
    const BufferIdManager::TargetBufferInfo& target_buffer) {
  decoder_channel_->ResumeAsync(ToGrpcTypes(target_buffer));
}

void ProxyCallTranslator::SetPlaybackRate(float rate) {
  decoder_channel_->SetPlaybackAsync(rate);
}

void ProxyCallTranslator::SetVolume(float multiplier) {
  decoder_channel_->SetVolumeAsync(multiplier);
}

bool ProxyCallTranslator::SetConfig(const AudioConfig& config) {
  return push_buffer_handler_.PushBuffer(ToGrpcTypes(config)) !=
         CmaBackend::BufferStatus::kBufferFailed;
}

void ProxyCallTranslator::UpdateTimestamp(
    const BufferIdManager::TargetBufferInfo& target_buffer) {
  decoder_channel_->UpdateTimestampAsync(ToGrpcTypes(target_buffer));
}

CmaBackend::BufferStatus ProxyCallTranslator::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer,
    BufferIdManager::BufferId buffer_id) {
  return push_buffer_handler_.PushBuffer(
      ToGrpcTypes(std::move(buffer), buffer_id));
}

std::optional<ProxyCallTranslator::PushBufferRequest>
ProxyCallTranslator::GetBufferedData() {
  return push_buffer_handler_.GetBufferedData();
}

bool ProxyCallTranslator::HasBufferedData() {
  return push_buffer_handler_.HasBufferedData();
}

void ProxyCallTranslator::HandleInitializeResponse(
    CastRuntimeAudioChannelBroker::StatusCode status) {
  HandleError(status);
}

void ProxyCallTranslator::HandleSetVolumeResponse(
    CastRuntimeAudioChannelBroker::StatusCode status) {
  HandleError(status);
}

void ProxyCallTranslator::HandleSetPlaybackResponse(
    CastRuntimeAudioChannelBroker::StatusCode status) {
  HandleError(status);
}

void ProxyCallTranslator::HandleStateChangeResponse(
    CastRuntimeAudioChannelBroker::Handler::PipelineState state,
    CastRuntimeAudioChannelBroker::StatusCode status) {
  if (!HandleError(status)) {
    return;
  }

  auto* task = new TaskRunner::CallbackTask<base::OnceClosure>(
      base::BindOnce(&ProxyCallTranslator::OnPipelineStateChangeTask,
                     weak_factory_.GetWeakPtr(), ToClientTypes(state)));
  client_task_runner_->PostTask(task, 0);
}

void ProxyCallTranslator::HandlePushBufferResponse(
    int64_t decoded_bytes,
    CastRuntimeAudioChannelBroker::StatusCode status) {
  if (!HandleError(status)) {
    return;
  }

  auto* task = new TaskRunner::CallbackTask<base::OnceClosure>(
      base::BindOnce(&ProxyCallTranslator::OnBytesDecodedTask,
                     weak_factory_.GetWeakPtr(), decoded_bytes));
  client_task_runner_->PostTask(task, 0);
}

void ProxyCallTranslator::HandleGetMediaTimeResponse(
    std::optional<MediaTime> time,
    CastRuntimeAudioChannelBroker::StatusCode status) {
  NOTREACHED();
}

bool ProxyCallTranslator::HandleError(
    CastRuntimeAudioChannelBroker::StatusCode status) {
  if (status == CastRuntimeAudioChannelBroker::StatusCode::kOk) {
    return true;
  }

  auto* task = new TaskRunner::CallbackTask<base::OnceClosure>(base::BindOnce(
      &ProxyCallTranslator::OnErrorTask, weak_factory_.GetWeakPtr()));
  client_task_runner_->PostTask(task, 0);
  return false;
}

void ProxyCallTranslator::OnErrorTask() {
  client_->OnError();
}

void ProxyCallTranslator::OnPipelineStateChangeTask(
    CmaProxyHandler::PipelineState state) {
  client_->OnPipelineStateChange(state);
}

void ProxyCallTranslator::OnBytesDecodedTask(int64_t decoded_byte_count) {
  client_->OnBytesDecoded(decoded_byte_count);
}

}  // namespace media
}  // namespace chromecast
