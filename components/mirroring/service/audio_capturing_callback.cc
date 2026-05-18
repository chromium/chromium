// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/audio_capturing_callback.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"

namespace mirroring {

AudioCapturingCallback::AudioCapturingCallback(
    AudioDataCallback audio_data_callback,
    ErrorCallback error_callback,
    mojo::Remote<mojom::SessionObserver>& observer)
    : audio_data_callback_(std::move(audio_data_callback)),
      error_callback_(std::move(error_callback)),
      logger_("AudioCapturingCallback", observer) {
  CHECK(!audio_data_callback_.is_null());
}

AudioCapturingCallback::~AudioCapturingCallback() = default;

void AudioCapturingCallback::OnCaptureStarted() {
  logger_.LogInfo("OnCaptureStarted");
}

void AudioCapturingCallback::Capture(const media::AudioBus* audio_bus,
                                     base::TimeTicks audio_capture_time,
                                     const media::AudioGlitchInfo& glitch_info,
                                     double volume) {
  if (!has_captured_) {
    logger_.LogInfo(base::StringPrintf("first Capture(): volume = %f", volume));
    has_captured_ = true;
  }
  // Note: We must copy the audio data here because the |audio_bus| provided
  // by the source is transient and we need to thread-hop this data to the
  // encoding thread. To do so safely, we must take ownership of the data.
  std::unique_ptr<media::AudioBus> captured_audio =
      media::AudioBus::Create(audio_bus->channels(), audio_bus->frames());
  audio_bus->CopyTo(captured_audio.get());
  audio_data_callback_.Run(std::move(captured_audio), audio_capture_time);
}

void AudioCapturingCallback::OnCaptureError(
    media::AudioCapturerSource::ErrorCode code,
    const std::string& message) {
  if (error_callback_) {
    std::move(error_callback_)
        .Run(base::StrCat({"AudioCaptureError occurred, code: ",
                           base::NumberToString(static_cast<int>(code)),
                           ", message: ", message}));
  }
}

void AudioCapturingCallback::OnCaptureMuted(bool is_muted) {
  logger_.LogInfo(base::StrCat(
      {"OnCaptureMuted, is_muted = ", base::NumberToString(is_muted)}));
}

}  // namespace mirroring
