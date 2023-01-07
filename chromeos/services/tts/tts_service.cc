// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_service.h"

#include <dlfcn.h>
#include <sys/resource.h>

#include "chromeos/services/tts/constants.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"

namespace chromeos {
namespace tts {

TtsService::TtsService(mojo::PendingReceiver<mojom::TtsService> receiver)
    : service_receiver_(this, std::move(receiver)) {
  if (setpriority(PRIO_PROCESS, 0, -10 /* real time audio */) != 0) {
    PLOG(ERROR) << "Unable to request real time priority; performance will be "
                   "impacted.";
  }
}

TtsService::~TtsService() = default;

void TtsService::BindGoogleTtsStream(
    mojo::PendingReceiver<mojom::GoogleTtsStream> receiver,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> factory) {
  google_tts_stream_ = std::make_unique<GoogleTtsStream>(
      this, std::move(receiver), std::move(factory));
}

void TtsService::BindPlaybackTtsStream(
    mojo::PendingReceiver<mojom::PlaybackTtsStream> receiver,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> factory,
    mojom::AudioParametersPtr desired_audio_parameters,
    BindPlaybackTtsStreamCallback callback) {
  media::AudioParameters params;

  if (desired_audio_parameters) {
    params =
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               media::ChannelLayoutConfig::Mono(),
                               desired_audio_parameters->sample_rate,
                               desired_audio_parameters->buffer_size);

    if (!params.IsValid()) {
      // Returning early disconnects the remote.
      return;
    }
  } else {
    // The client did not specify parameters; use defaults.
    params =
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               media::ChannelLayoutConfig::Mono(),
                               kDefaultSampleRate, kDefaultBufferSize);
  }
  DCHECK(params.IsValid());
  playback_tts_stream_ = std::make_unique<PlaybackTtsStream>(
      this, std::move(receiver), std::move(factory), params);

  auto ret_params = mojom::AudioParameters::New();
  ret_params->sample_rate = params.sample_rate();
  ret_params->buffer_size = params.frames_per_buffer();
  std::move(callback).Run(std::move(ret_params));
}

void TtsService::MaybeExit() {
  if ((!google_tts_stream_ || !google_tts_stream_->IsBound()) &&
      (!playback_tts_stream_ || !playback_tts_stream_->IsBound())) {
    service_receiver_.reset();
    if (!keep_process_alive_for_testing_) {
      if (google_tts_stream_)
        google_tts_stream_->set_is_in_process_teardown(true);
      exit(0);
    }
  }
}

}  // namespace tts
}  // namespace chromeos
