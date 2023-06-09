// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_CAPTURER_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_CAPTURER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_stream_factory.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace recording {

// Defines the type of the callback that will be triggered repeatedly by the
// audio input device to deliver a stream of buffers containing the captured
// audio data. Each call will provide an `audio_bus` and the
// `audio_capture_time` when the first frame of that bus was captured.
// This callback will be invoked on a worker thread created by the audio input
// device (`media::AudioDeviceThread`). The provided `audio_bus` owns its own
// memory.
using OnAudioCapturedCallback =
    base::RepeatingCallback<void(std::unique_ptr<media::AudioBus> audio_bus,
                                 base::TimeTicks audio_capture_time)>;

// Defines an audio capturer that can capture an audio input device whose ID is
// `device_id`. The provided `audio_stream_factory` will be used so that the
// underlying `AudioInputDevice` can communicate with audio service via IPC. The
// provided `audio_params` will be used to initialize the underlying audio
// capturer. `callback` will be invoked according to the rules specified above.
class AudioCapturer : public media::AudioCapturerSource::CaptureCallback {
 public:
  AudioCapturer(base::StringPiece device_id,
                mojo::PendingRemote<media::mojom::AudioStreamFactory>
                    audio_stream_factory,
                const media::AudioParameters& audio_params,
                OnAudioCapturedCallback callback);
  AudioCapturer(const AudioCapturer&) = delete;
  AudioCapturer& operator=(const AudioCapturer&) = delete;
  ~AudioCapturer() override;

  // Starts and stops the audio capture.
  void Start();
  void Stop();

  // media::AudioCapturerSource::CaptureCallback:
  void OnCaptureStarted() override;
  void Capture(const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) override;
  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) override;
  void OnCaptureMuted(bool is_muted) override;

 private:
  scoped_refptr<media::AudioCapturerSource> audio_capturer_;

  const OnAudioCapturedCallback on_audio_captured_callback_;
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_CAPTURER_H_
