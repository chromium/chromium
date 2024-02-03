// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_STREAM_MIXER_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_STREAM_MIXER_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "media/mojo/mojom/audio_stream_factory.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace capture_mode {
class AudioCapturer;
}  // namespace capture_mode

namespace media {
class AudioBus;
}  // namespace media

namespace recording {

class AudioStream;

// Defines a type for the callback that the mixer uses to provide the mixed
// audio bus output to its client.
using OnAudioMixerOutputCallback =
    base::RepeatingCallback<void(std::unique_ptr<media::AudioBus> audio_bus,
                                 base::TimeTicks audio_capture_time)>;

// Defines an audio stream mixer which can be used to create audio capturers
// whose `device_id`s are given to `AddAudioCapturer()`. Each capturer will be
// associated with an `AudioStream` instance that will queue all the unmixed /
// unconsumed audio buses received so far from the corresponding audio capturer.
// Once possible, all mix-able audio frames from all audio streams will be mixed
// and consumed, and will be provided to the client via the given `callback` as
// a single audio bus that contains all the mixed audio frames so far.
// All the operations of this class, including construction and destruction must
// happen on the same sequence.
class AudioStreamMixer {
 private:
  using PassKey = base::PassKey<AudioStreamMixer>;

 public:
  explicit AudioStreamMixer(PassKey);
  // A constructor used only by tests.
  explicit AudioStreamMixer(PassKey, OnAudioMixerOutputCallback callback);
  AudioStreamMixer(const AudioStreamMixer&) = delete;
  AudioStreamMixer& operator=(const AudioStreamMixer&) = delete;
  ~AudioStreamMixer();

  // Creates an instance of the mixer that is bound to the given `task_runner`.
  // All the operations of the mixer, including its construction and destruction
  // will be done on this `task_runner`.
  static base::SequenceBound<AudioStreamMixer> Create(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Creates a new `AudioCapturer` and a corresponding `AudioStream` to capture
  // the audio input device whose ID is the given `device_id`. The pending
  // remote will be used to communicate with the audio stream factory in the
  // audio service. Depending on the values of `use_automatic_gain_control` and
  // `use_echo_canceller`, automatic gain control and echo cancelation will be
  // used respectively.
  // The captured audio frames from this device will be mixed with the captured
  // audio frames from all audio capturers managed by this mixer. The mixed
  // output will be provided to the client via the `callback` given to the
  // constructor.
  void AddAudioCapturer(std::string_view device_id,
                        mojo::PendingRemote<media::mojom::AudioStreamFactory>
                            audio_stream_factory,
                        bool use_automatic_gain_control,
                        bool use_echo_canceller);

  // Starts and stops capturing the so-far added audio capturers. `callback`
  // will be called repeatedly to provide the output mixed audio buses.
  void Start(OnAudioMixerOutputCallback callback);
  void Stop();

 private:
  friend class RecordingServiceTestApi;
  friend class AudioStreamMixerTest;

  static PassKey PassKeyForTesting();

  // Returns the number of audio capturers managed by this mixer.
  int GetNumberOfCapturers() const;

  // Will be called by the audio capturer that corresponds with the given
  // `audio_stream` to provide an `audio_bus` that was captured at
  // `audio_capture_time`. This `audio_bus` will be appended to `audio_stream`
  // and a mix and consume attempt will be made. If possible, audio frames from
  // all the managed audio streams will be mixed and consumed to provide an
  // output audio bus to the client.
  void OnAudioCaptured(AudioStream* audio_stream,
                       std::unique_ptr<media::AudioBus> audio_bus,
                       base::TimeTicks audio_capture_time);

  // Attempts to mix the available audio frames from all managed audio streams,
  // and if successful, a new audio bus containing the mixed output will be
  // provided to the client. If `flush` is true, all the available frames in all
  // streams will be mixed together and provided to the client regardless of the
  // overlap.
  void MaybeMixAndOutput(bool flush);

  // Creates and returns an audio bus that is big enough to contain all the
  // mixable audio frames from all the managed audio streams.
  // `out_bus_timestamp` will be filled with the timestamp that should be used
  // as the capture time of the mixer bus (which is the timestamp of the
  // earliest audio frame that's being mixed).
  // If nothing can be mixed at the moment (e.g. not all streams have frames),
  // `nullptr` will be returned.
  // If `flush` is true, it returns an audio bus that spans from the beginning
  // of the earliest frame in all streams, to the end of the latest frame in all
  // streams, so that it can be used to mix all frames available in all streams.
  std::unique_ptr<media::AudioBus> CreateMixerBus(
      bool flush,
      base::TimeTicks& out_bus_timestamp) const;

  SEQUENCE_CHECKER(sequence_checker_);

  // The callback that will be called repeatedly to provide the client with the
  // mixed audio buses.
  OnAudioMixerOutputCallback on_mixer_output_callback_;

  // A list of audio capturers and their corresponding audio streams.
  std::vector<std::unique_ptr<AudioStream>> streams_;
  std::vector<std::unique_ptr<capture_mode::AudioCapturer>> audio_capturers_;

  base::WeakPtrFactory<AudioStreamMixer> weak_ptr_factory_{this};
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_STREAM_MIXER_H_
