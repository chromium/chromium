// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/control/remoting/rpc_demuxer_stream_handler.h"

#include <sstream>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "media/cast/openscreen/remoting_message_factories.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

using openscreen::cast::RpcMessenger;

namespace cast_streaming::remoting {
namespace {

// The number of frames requested in each ReadUntil RPC message.
constexpr int kNumFramesInEachReadUntil = 16;

// Minimum frequency with which RPC_DS_READUNTIL RPC messages may be sent.
// Mainly exists to avoid creating a new READUNTIL call after each individual
// frame is read, if they are read faster than they are received from the
// sender.
//
// NOTE: This may cause a few hundred extra milliseconds of delay following a
// FLUSHUNTIL call, but this will not cause any user-visible issues.
constexpr base::TimeDelta kMinReadUntilCallFrequency = base::Milliseconds(100);

// The maximum amount of time allowed to a request before it is assumed to
// have been dropped.
constexpr base::TimeDelta kRequestTimeout = base::Milliseconds(500);

}  // namespace

RpcDemuxerStreamHandler::RpcDemuxerStreamHandler(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    Client* client,
    HandleFactory handle_factory,
    RpcProcessMessageCB process_message_cb)
    : task_runner_(std::move(task_runner)),
      client_(client),
      handle_factory_(std::move(handle_factory)),
      process_message_cb_(std::move(process_message_cb)) {
  DCHECK(task_runner_);
  DCHECK(handle_factory_);
  DCHECK(process_message_cb_);
}

RpcDemuxerStreamHandler::~RpcDemuxerStreamHandler() = default;

void RpcDemuxerStreamHandler::OnRpcAcquireDemuxer(
    RpcMessenger::Handle audio_stream_handle,
    RpcMessenger::Handle video_stream_handle) {
  // Initialization of the Demuxer happens automatically, so immediately
  // initialize the DemuxerStreams.
  if (audio_stream_handle != RpcMessenger::kInvalidHandle) {
    audio_message_processor_ = std::make_unique<MessageProcessor>(
        task_runner_, client_, process_message_cb_, handle_factory_.Run(),
        audio_stream_handle, MessageProcessor::Type::kAudio);
    std::unique_ptr<openscreen::cast::RpcMessage> message =
        media::cast::CreateMessageForDemuxerStreamInitialize(
            audio_message_processor_->local_handle());
    process_message_cb_.Run(audio_message_processor_->remote_handle(),
                            std::move(message));
  }

  if (video_stream_handle != RpcMessenger::kInvalidHandle) {
    video_message_processor_ = std::make_unique<MessageProcessor>(
        task_runner_, client_, process_message_cb_, handle_factory_.Run(),
        video_stream_handle, MessageProcessor::Type::kVideo);
    std::unique_ptr<openscreen::cast::RpcMessage> message =
        media::cast::CreateMessageForDemuxerStreamInitialize(
            video_message_processor_->local_handle());
    process_message_cb_.Run(video_message_processor_->remote_handle(),
                            std::move(message));
  }
}

void RpcDemuxerStreamHandler::OnRpcEnableBitstreamConverterCallback(
    RpcMessenger::Handle handle,
    bool succeeded) {
  if (audio_message_processor_ &&
      handle == audio_message_processor_->local_handle()) {
    audio_message_processor_->OnBitstreamConverterEnabled(succeeded);
  } else if (video_message_processor_ &&
             handle == video_message_processor_->local_handle()) {
    video_message_processor_->OnBitstreamConverterEnabled(succeeded);
  } else {
    std::stringstream logstream;
    logstream
        << "OnRpcEnableBitstreamConverterCallback received for invalid handle "
        << handle << ". Valid handles are";
    if (audio_message_processor_) {
      logstream << " '" << audio_message_processor_->local_handle()
                << "'' for audio";
    }
    if (video_message_processor_) {
      logstream << " '" << video_message_processor_->local_handle()
                << "'' for video";
    }

    LOG(WARNING) << logstream.str();
  }
}

void RpcDemuxerStreamHandler::OnRpcInitializeCallback(
    RpcMessenger::Handle handle,
    std::optional<media::AudioDecoderConfig> audio_config,
    std::optional<media::VideoDecoderConfig> video_config) {
  if (audio_message_processor_ &&
      handle == audio_message_processor_->local_handle()) {
    audio_message_processor_->OnRpcInitializeCallback(std::move(audio_config),
                                                      std::move(video_config));
  } else if (video_message_processor_ &&
             handle == video_message_processor_->local_handle()) {
    video_message_processor_->OnRpcInitializeCallback(std::move(audio_config),
                                                      std::move(video_config));
  } else {
    LOG(WARNING) << "OnRpcInitializeCallback received for invalid handle";
  }
}

void RpcDemuxerStreamHandler::OnRpcReadUntilCallback(
    RpcMessenger::Handle handle,
    std::optional<media::AudioDecoderConfig> audio_config,
    std::optional<media::VideoDecoderConfig> video_config,
    uint32_t total_frames_received) {
  if (audio_message_processor_ &&
      handle == audio_message_processor_->local_handle()) {
    audio_message_processor_->OnRpcReadUntilCallback(std::move(audio_config),
                                                     std::move(video_config),
                                                     total_frames_received);
  } else if (video_message_processor_ &&
             handle == video_message_processor_->local_handle()) {
    video_message_processor_->OnRpcReadUntilCallback(std::move(audio_config),
                                                     std::move(video_config),
                                                     total_frames_received);
  } else {
    LOG(WARNING) << "OnRpcReadUntilCallback received for invalid handle";
  }
}

void RpcDemuxerStreamHandler::OnRpcBitstreamConverterEnabled(
    RpcMessenger::Handle handle,
    bool success) {
  if (audio_message_processor_ &&
      handle == audio_message_processor_->local_handle()) {
    audio_message_processor_->OnBitstreamConverterEnabled(success);
  } else if (video_message_processor_ &&
             handle == video_message_processor_->local_handle()) {
    video_message_processor_->OnBitstreamConverterEnabled(success);
  } else {
    LOG(WARNING)
        << "OnRpcBitstreamConverterEnabled received for invalid handle";
  }
}

base::WeakPtr<DemuxerStreamClient> RpcDemuxerStreamHandler::GetAudioClient() {
  if (!audio_message_processor_) {
    return nullptr;
  }

  return audio_message_processor_->GetWeakPtr();
}

base::WeakPtr<DemuxerStreamClient> RpcDemuxerStreamHandler::GetVideoClient() {
  if (!video_message_processor_) {
    return nullptr;
  }

  return video_message_processor_->GetWeakPtr();
}

RpcDemuxerStreamHandler::Client::~Client() = default;

RpcDemuxerStreamHandler::MessageProcessor::MessageProcessor(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    Client* client,
    RpcProcessMessageCB process_message_cb,
    RpcMessenger::Handle local_handle,
    RpcMessenger::Handle remote_handle,
    Type type)
    : task_runner_(std::move(task_runner)),
      client_(client),
      process_message_cb_(std::move(process_message_cb)),
      local_handle_(local_handle),
      remote_handle_(remote_handle),
      type_(type),
      weak_factory_(this) {
  DCHECK(task_runner_);
  DCHECK(client_);
  DCHECK_NE(local_handle_, RpcMessenger::kInvalidHandle);
  DCHECK_NE(remote_handle_, RpcMessenger::kInvalidHandle);
}

RpcDemuxerStreamHandler::MessageProcessor::~MessageProcessor() = default;

bool RpcDemuxerStreamHandler::MessageProcessor::OnRpcInitializeCallback(
    std::optional<media::AudioDecoderConfig> audio_config,
    std::optional<media::VideoDecoderConfig> video_config) {
  if (audio_config && type_ != Type::kAudio) {
    LOG(WARNING) << "Received an audio config for a video DemuxerStream";
    return false;
  } else if (video_config && type_ != Type::kVideo) {
    LOG(WARNING) << "Received a video config for an audio DemuxerStream";
    return false;
  }

  if (audio_config) {
    client_->OnNewAudioConfig(std::move(audio_config.value()));
  } else if (video_config) {
    client_->OnNewVideoConfig(std::move(video_config.value()));
  }

  return true;
}

bool RpcDemuxerStreamHandler::MessageProcessor::OnRpcReadUntilCallback(
    std::optional<media::AudioDecoderConfig> audio_config,
    std::optional<media::VideoDecoderConfig> video_config,
    uint32_t total_frames_received) {
  call_timeout_timer_.Stop();
  failed_consecutive_read_until_requests_ = 0;
  is_read_until_call_ongoing_ = false;

  // Handle config changes.
  if (!OnRpcInitializeCallback(std::move(audio_config),
                               std::move(video_config))) {
    LOG(WARNING) << "Failed to process OnRpcReadUntilCallback.";

    // If we got an invalid config, something went wrong. Give the sender a
    // chance to send a working config rather than deadlocking.
    OnNoBuffersAvailable();
    return false;
  }

  total_frames_received_ = total_frames_received;

  // If a call was pending, make that call now.
  if (is_read_until_call_pending_) {
    DVLOG(1) << "Executing pending buffer request";
    OnNoBuffersAvailable();
  }

  return true;
}

void RpcDemuxerStreamHandler::MessageProcessor::OnBitstreamConverterEnabled(
    bool success) {
  if (!bitstream_converter_enabled_cb_) {
    return;
  }

  std::move(bitstream_converter_enabled_cb_).Run(success);
}

base::WeakPtr<RpcDemuxerStreamHandler::MessageProcessor>
RpcDemuxerStreamHandler::MessageProcessor::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void RpcDemuxerStreamHandler::MessageProcessor::EnableBitstreamConverter(
    BitstreamConverterEnabledCB cb) {
  DCHECK(!bitstream_converter_enabled_cb_);
  bitstream_converter_enabled_cb_ = std::move(cb);

  auto message =
      media::cast::CreateMessageForDemuxerStreamEnableBitstreamConverter();
  process_message_cb_.Run(remote_handle(), std::move(message));
}

void RpcDemuxerStreamHandler::MessageProcessor::OnNoBuffersAvailable() {
  DVLOG(1) << "Requesting more buffers: source handle = " << local_handle_
           << ", remote handle = " << remote_handle_;

  // If a call is already ongoing, queue up a new call to be made once its ACK
  // is received.
  if (is_read_until_call_ongoing_) {
    is_read_until_call_pending_ = true;
    DVLOG(1) << "Queueing request because one is already ongoing";
    return;
  }

  is_read_until_call_pending_ = false;
  is_read_until_call_ongoing_ = true;

  // If requests have failed, its possible that either the requests was lost or
  // the response was lost. In the latter case, creating the same call again
  // would have no effect so instead request more buffers if the previous
  // request failed.
  const int requests_to_account_for =
      1 + failed_consecutive_read_until_requests_;
  const int frames_to_request =
      kNumFramesInEachReadUntil * requests_to_account_for;
  auto message = media::cast::CreateMessageForDemuxerStreamReadUntil(
      local_handle(), total_frames_received() + frames_to_request);

  // Only call every |kMinReadUntilCallFrequency| at most.
  const auto now = base::TimeTicks::Now();
  auto remaining_time = (last_request_time_ + kMinReadUntilCallFrequency) - now;
  if (remaining_time < base::Microseconds(0)) {
    remaining_time = base::Microseconds(0);
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(process_message_cb_, remote_handle(), std::move(message)),
      remaining_time);
  last_request_time_ = now + remaining_time;

  // Start a timer to handle the case of dropped requests or ACKS, so a retry
  // may be made.
  call_timeout_timer_.Start(
      FROM_HERE, kRequestTimeout + remaining_time,
      base::BindOnce(
          &RpcDemuxerStreamHandler::MessageProcessor::OnBufferRequestTimeout,
          weak_factory_.GetWeakPtr()));
}

void RpcDemuxerStreamHandler::MessageProcessor::OnError() {
  auto message = media::cast::CreateMessageForDemuxerStreamError();
  process_message_cb_.Run(remote_handle(), std::move(message));
}

void RpcDemuxerStreamHandler::MessageProcessor::OnBufferRequestTimeout() {
  LOG(WARNING) << "READUNTIL rpc call dropped. Retrying...";

  // NOTE: No need to persist |is_read_until_call_pending_| because the batch
  // size increases for retries.
  failed_consecutive_read_until_requests_++;
  is_read_until_call_ongoing_ = false;
  OnNoBuffersAvailable();
}

}  // namespace cast_streaming::remoting
