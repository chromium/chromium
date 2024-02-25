// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_receiver_session_client.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/streaming_controller_base.h"
#include "components/cast_streaming/common/public/cast_streaming_url.h"
#include "media/base/video_decoder_config.h"

namespace cast_receiver {

constexpr base::TimeDelta
    StreamingReceiverSessionClient::kMaxAVSettingsWaitTime;

StreamingReceiverSessionClient::StreamingReceiverSessionClient(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    network::NetworkContextGetter network_context_getter,
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    content::WebContents* web_contents,
    Handler* handler,
    cast_receiver::StreamingConfigManager* config_manager,
    bool supports_audio,
    bool supports_video)
    : StreamingReceiverSessionClient(
          std::move(task_runner),
          std::move(network_context_getter),
          StreamingControllerBase::Create(std::move(message_port),
                                          web_contents),
          handler,
          config_manager,
          supports_audio,
          supports_video) {}

StreamingReceiverSessionClient::StreamingReceiverSessionClient(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    network::NetworkContextGetter network_context_getter,
    std::unique_ptr<StreamingController> streaming_controller,
    Handler* handler,
    cast_receiver::StreamingConfigManager* config_manager,
    bool supports_audio,
    bool supports_video)
    : handler_(handler),
      task_runner_(std::move(task_runner)),
      streaming_controller_(std::move(streaming_controller)),
      supports_audio_(supports_audio),
      supports_video_(supports_video),
      weak_factory_(this) {
  DCHECK(handler_);
  DCHECK(task_runner_);
  DCHECK(!network_context_getter.is_null());
  DCHECK(config_manager);

  cast_streaming::SetNetworkContextGetter(std::move(network_context_getter));

  DVLOG(1) << "Streaming Receiver Session start pending...";
  config_manager->AddConfigObserver(*this);
  if (config_manager->has_config()) {
    // TODO(crbug.com/1359568): This may not behave correctly if the config is
    // updated before the pushed task runs.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StreamingReceiverSessionClient::OnStreamingConfigSet,
                       weak_factory_.GetWeakPtr(), config_manager->config()));
    return;
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&StreamingReceiverSessionClient::VerifyAVSettingsReceived,
                     weak_factory_.GetWeakPtr()),
      kMaxAVSettingsWaitTime);
}

StreamingReceiverSessionClient::~StreamingReceiverSessionClient() {
  DVLOG(1) << "StreamingReceiverSessionClient state when destroyed"
           << "\n\tIs Healthy: " << is_healthy()
           << "\n\tLaunch called: " << is_streaming_launch_pending()
           << "\n\tAV Settings Received: " << has_received_av_settings();

  cast_streaming::SetNetworkContextGetter({});
}

StreamingReceiverSessionClient::Handler::~Handler() = default;

void StreamingReceiverSessionClient::LaunchStreamingReceiverAsync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_streaming_launch_pending());

  streaming_state_ |= LaunchState::kLaunchCalled;
  streaming_controller_->StartPlaybackAsync(
      base::BindOnce(&StreamingReceiverSessionClient::OnPlaybackStarted,
                     weak_factory_.GetWeakPtr()));
}

void StreamingReceiverSessionClient::VerifyAVSettingsReceived() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (streaming_state_ & LaunchState::kAVSettingsReceived) {
    return;
  }

  LOG(ERROR) << "AVSettings not received within the allocated amount of time";
  TriggerError();
}

void StreamingReceiverSessionClient::OnPlaybackStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  streaming_state_ |= LaunchState::kLaunched;
  handler_->OnStreamingSessionStarted();
}

void StreamingReceiverSessionClient::OnStreamingConfigSet(
    const cast_streaming::ReceiverConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make a copy so that it can be mofidied locally.
  cast_streaming::ReceiverConfig constraints = config;

  if (streaming_state_ == LaunchState::kError) {
    LOG(WARNING) << "Config received after an error!";
    return;
  }

  if (!supports_audio_) {
    LOG(WARNING) << "Disallowing audio for this streaming session!";
    constraints.audio_codecs.clear();
    constraints.audio_limits.clear();
  }
  if (!supports_video_) {
    LOG(WARNING) << "Disallowing video for this streaming session!";
    constraints.video_codecs.clear();
    constraints.video_limits.clear();
  }
  if (supports_audio_ && supports_video_) {
    DVLOG(1) << "Allowing both audio and video for this streaming session!";
  }

  streaming_state_ |= LaunchState::kAVSettingsReceived;
  if (!has_streaming_launched()) {
    streaming_controller_->InitializeReceiverSession(config, this);
    return;
  }

  // TODO(crbug.com/1359568): Handle receiving a new config after streaming has
  // already started.
  LOG(WARNING)
      << "Received updated streaming config during an ongoing session!";
}

void StreamingReceiverSessionClient::OnAudioConfigUpdated(
    const ::media::AudioDecoderConfig& audio_config) {}

void StreamingReceiverSessionClient::OnVideoConfigUpdated(
    const ::media::VideoDecoderConfig& video_config) {
  handler_->OnResolutionChanged(video_config.visible_rect(),
                                video_config.video_transformation());
}

void StreamingReceiverSessionClient::OnStreamingSessionEnded() {
  // The streaming session will only "end" (as opposed to being "renegotated"
  // when a new config is sent or the stream changes between mirroring and
  // remoting) when the session completely exits. This occurs either when there
  // is an error in the runtime or the when sender-side ends the streaming
  // session.
  //
  // In either case, the result is an unsupported state for the
  // StreamingRuntimeApplication, so an error.
  TriggerError();
}

void StreamingReceiverSessionClient::TriggerError() {
  if (!is_healthy()) {
    return;
  }

  streaming_state_ |= LaunchState::kError;
  handler_->OnError();
}

}  // namespace cast_receiver
