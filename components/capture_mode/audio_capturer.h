// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTURE_MODE_AUDIO_CAPTURER_H_
#define COMPONENTS_CAPTURE_MODE_AUDIO_CAPTURER_H_

#include <memory>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/capture_mode/capture_mode_export.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_bus_pool.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_stream_factory.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace capture_mode {

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
class CAPTURE_MODE_EXPORT AudioCapturer
    : public media::AudioCapturerSource::CaptureCallback {
 public:
  AudioCapturer(std::string_view device_id,
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
               const media::AudioGlitchInfo& glitch_info,
               double volume,
               bool key_pressed) override;
  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) override;
  void OnCaptureMuted(bool is_muted) override;

 private:
  // Will be called when the audio bus that we send to the client via
  // `on_audio_captured_callback_` (which in turn wraps the `backing_audio_bus`)
  // is destroyed. The `backing_audio_bus` can then be inserted back into the
  // `audio_bus_pool_`.
  void OnAudioBusDone(std::unique_ptr<media::AudioBus> backing_audio_bus);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<media::AudioCapturerSource> audio_capturer_;

  const OnAudioCapturedCallback on_audio_captured_callback_;

  // An audio bus pool which we use to avoid allocating audio buses on the real-
  // time audio thread when `Capture()` above is called.
  media::AudioBusPoolImpl audio_bus_pool_;

  // This will be initialized in the ctor as a weak ptr to `this`, and will be
  // used to bind a callback to `OnAudioBusDone()`. The creation of this weak
  // ptr, its invalidation, and the invocation of the callback bound to
  // `OnAudioBusDone()` will all be done on the same sequence guarded by the
  // above `sequence_checker_`.
  base::WeakPtr<AudioCapturer> weak_ptr_this_;

  base::WeakPtrFactory<AudioCapturer> weak_ptr_factory_{this};
};

}  // namespace capture_mode

#endif  // COMPONENTS_CAPTURE_MODE_AUDIO_CAPTURER_H_
