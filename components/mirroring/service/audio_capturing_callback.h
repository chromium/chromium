// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_AUDIO_CAPTURING_CALLBACK_H_
#define COMPONENTS_MIRRORING_SERVICE_AUDIO_CAPTURING_CALLBACK_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/service/mirroring_logger.h"
#include "media/base/audio_capturer_source.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
class AudioBus;
}

namespace mirroring {

// Receives data from the audio capturer source, and calls `audio_data_callback`
// when new data is available.
class COMPONENT_EXPORT(MIRRORING_SERVICE) AudioCapturingCallback final
    : public media::AudioCapturerSource::CaptureCallback {
 public:
  using AudioDataCallback =
      base::RepeatingCallback<void(std::unique_ptr<media::AudioBus> audio_bus,
                                   base::TimeTicks recorded_time)>;

  // NOTE: the caller is expected to take ownership of the error message, since
  // we cannot otherwise make any guarantees about its lifetime.
  using ErrorCallback = base::OnceCallback<void(std::string)>;

  AudioCapturingCallback(AudioDataCallback audio_data_callback,
                         ErrorCallback error_callback,
                         mojo::Remote<mojom::SessionObserver>& observer);

  AudioCapturingCallback(const AudioCapturingCallback&) = delete;
  AudioCapturingCallback& operator=(const AudioCapturingCallback&) = delete;

  ~AudioCapturingCallback() override;

 private:
  // media::AudioCapturerSource::CaptureCallback implementation.
  void OnCaptureStarted() override;
  void Capture(const media::AudioBus* audio_bus,
               base::TimeTicks audio_capture_time,
               const media::AudioGlitchInfo& glitch_info,
               double volume) override;
  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) override;
  void OnCaptureMuted(bool is_muted) override;

  const AudioDataCallback audio_data_callback_;
  ErrorCallback error_callback_;
  MirroringLogger logger_;
  bool has_captured_ = false;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_AUDIO_CAPTURING_CALLBACK_H_
