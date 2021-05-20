// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_service.h"

#include <dlfcn.h>
#include <sys/resource.h>

#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"

namespace chromeos {
namespace tts {

// TODO: remove this once dynamic params are supported.
namespace {
constexpr int kDefaultSampleRate = 24000;
constexpr int kDefaultBufferSize = 512;
}  // namespace

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
    BindPlaybackTtsStreamCallback callback) {
  // TODO(accessibility): make it possible to change this dynamically by passing
  // params from extension manifest.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_MONO, kDefaultSampleRate,
                                kDefaultBufferSize);

  playback_tts_stream_ = std::make_unique<PlaybackTtsStream>(
      this, std::move(receiver), std::move(factory), params);
  std::move(callback).Run(kDefaultSampleRate, kDefaultBufferSize);
}

void TtsService::MaybeExit() {
  if ((!google_tts_stream_ || !google_tts_stream_->IsBound()) &&
      (!playback_tts_stream_ || !playback_tts_stream_->IsBound())) {
    service_receiver_.reset();
    if (!keep_process_alive_for_testing_)
      exit(0);
  }
}

}  // namespace tts
}  // namespace chromeos
