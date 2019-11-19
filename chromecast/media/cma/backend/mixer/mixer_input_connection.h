// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_CONNECTION_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_CONNECTION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/cma/backend/audio_fader.h"
#include "chromecast/media/cma/backend/mixer/mixer_input.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {
class AudioBufferMemoryPool;
class AudioBus;
class AudioRendererAlgorithm;
}  // namespace media

namespace net {
class IOBuffer;
}  // namespace net

namespace chromecast {
class IOBufferPool;

namespace media {
class StreamMixer;

namespace mixer_service {
class Generic;
class OutputStreamParams;
}  // namespace mixer_service

// A mixer source that receives audio data from a mixer service connection and
// buffers/fades it as requested before feeding it to the mixer. Must be created
// on an IO thread. This class manages its own lifetime and should not be
// externally deleted.
class MixerInputConnection : public mixer_service::MixerSocket::Delegate,
                             public MixerInput::Source,
                             public AudioFader::Source {
 public:
  using RenderingDelay = MediaPipelineBackend::AudioDecoder::RenderingDelay;

  MixerInputConnection(StreamMixer* mixer,
                       std::unique_ptr<mixer_service::MixerSocket> socket,
                       const mixer_service::OutputStreamParams& params);

  // Only public to allow task_runner->DeleteSoon() to work.
  ~MixerInputConnection() override;

 private:
  friend class MixerServiceReceiver;

  enum class State {
    kUninitialized,   // Not initialized by the mixer yet.
    kNormalPlayback,  // Normal playback.
    kGotEos,          // Got the end-of-stream buffer (normal playback).
    kSignaledEos,     // Sent EOS signal up to delegate.
    kRemoved,         // The caller has removed this source; finish playing out.
  };

  // mixer_service::MixerSocket::Delegate implementation:
  bool HandleMetadata(const mixer_service::Generic& message) override;
  bool HandleAudioData(char* data, int size, int64_t timestamp) override;
  bool HandleAudioBuffer(scoped_refptr<net::IOBuffer> buffer,
                         char* data,
                         int size,
                         int64_t timestamp) override;
  void OnConnectionError() override;

  void CreateBufferPool(int frame_count);
  void OnInactivityTimeout();
  void RestartPlaybackAt(int64_t timestamp, int64_t pts);
  void SetMediaPlaybackRate(double rate);
  void SetPaused(bool paused);

  // MixerInput::Source implementation:
  int num_channels() override;
  int input_samples_per_second() override;
  bool primary() override;
  const std::string& device_id() override;
  AudioContentType content_type() override;
  int desired_read_size() override;
  int playout_channel() override;
  bool active() override;

  void InitializeAudioPlayback(int read_size,
                               RenderingDelay initial_rendering_delay) override;
  int FillAudioPlaybackFrames(int num_frames,
                              RenderingDelay rendering_delay,
                              ::media::AudioBus* buffer) override;
  void OnAudioPlaybackError(MixerError error) override;
  void FinalizeAudioPlayback() override;

  // AudioFader::Source implementation:
  int FillFaderFrames(int num_frames,
                      RenderingDelay rendering_delay,
                      float* const* channels)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) override;

  int FillAudio(int num_frames, float* const* channels)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void WritePcm(scoped_refptr<net::IOBuffer> data);
  int64_t QueueData(scoped_refptr<net::IOBuffer> data)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void PostPcmCompletion();
  void PostEos();
  void PostError(MixerError error);
  void PostAudioReadyForPlayback();
  void DropAudio(int64_t frames) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void CheckAndStartPlaybackIfNecessary(int num_frames,
                                        int64_t playback_absolute_timestamp)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  StreamMixer* const mixer_;
  std::unique_ptr<mixer_service::MixerSocket> socket_;

  const bool ignore_for_stream_count_;
  const int fill_size_;
  const int algorithm_fill_size_;
  const int num_channels_;
  const int input_samples_per_second_;
  const mixer_service::SampleFormat sample_format_;
  const bool primary_;
  const std::string device_id_;
  const AudioContentType content_type_;
  const int playout_channel_;

  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  int max_queued_frames_;
  // Minimum number of frames buffered before starting to fill data.
  int start_threshold_frames_;

  scoped_refptr<IOBufferPool> buffer_pool_;

  // Only used on the IO thread.
  bool audio_ready_for_playback_fired_ = false;
  base::OneShotTimer inactivity_timer_;
  bool connection_error_ = false;
  int buffer_pool_frames_ = 0;

  base::Lock lock_;
  State state_ GUARDED_BY(lock_) = State::kUninitialized;
  bool paused_ GUARDED_BY(lock_) = false;
  bool mixer_error_ GUARDED_BY(lock_) = false;
  scoped_refptr<net::IOBuffer> pending_data_ GUARDED_BY(lock_);
  base::circular_deque<scoped_refptr<net::IOBuffer>> queue_ GUARDED_BY(lock_);
  int queued_frames_ GUARDED_BY(lock_) = 0;
  RenderingDelay mixer_rendering_delay_ GUARDED_BY(lock_);
  int64_t next_playback_timestamp_ GUARDED_BY(lock_) = INT64_MIN;
  int mixer_read_size_ GUARDED_BY(lock_) = 0;
  int extra_delay_frames_ GUARDED_BY(lock_) = 0;
  int current_buffer_offset_ GUARDED_BY(lock_) = 0;
  AudioFader fader_ GUARDED_BY(lock_);
  bool zero_fader_frames_ GUARDED_BY(lock_) = false;
  bool started_ GUARDED_BY(lock_) = false;
  double playback_rate_ GUARDED_BY(lock_) = 1.0;
  bool use_start_timestamp_ GUARDED_BY(lock_) = false;
  // The absolute timestamp relative to clock monotonic (raw) at which the
  // playback should start. INT64_MIN indicates playback should start ASAP.
  // INT64_MAX indicates playback should start at a specified timestamp,
  // but we don't know what that timestamp is.
  int64_t playback_start_timestamp_ GUARDED_BY(lock_) = INT64_MIN;
  // The PTS the playback should start at. We will drop audio pushed to us
  // with PTS values below this value. If the audio doesn't have a starting
  // PTS, then this value can be INT64_MIN, to play whatever audio is sent
  // to us.
  int64_t playback_start_pts_ GUARDED_BY(lock_) = INT64_MIN;
  int remaining_silence_frames_ GUARDED_BY(lock_) = 0;
  std::unique_ptr<::media::AudioRendererAlgorithm> rate_shifter_
      GUARDED_BY(lock_);
  std::unique_ptr<::media::AudioBus> rate_shifter_output_ GUARDED_BY(lock_);
  int64_t rate_shifter_input_frames_ GUARDED_BY(lock_) = 0;
  int64_t rate_shifter_output_frames_ GUARDED_BY(lock_) = 0;
  bool skip_next_fill_for_rate_change_ GUARDED_BY(lock_) = false;
  scoped_refptr<::media::AudioBufferMemoryPool> audio_buffer_pool_;

  base::RepeatingClosure pcm_completion_task_;
  base::RepeatingClosure eos_task_;
  base::RepeatingClosure ready_for_playback_task_;

  base::WeakPtr<MixerInputConnection> weak_this_;
  base::WeakPtrFactory<MixerInputConnection> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MixerInputConnection);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_CONNECTION_H_
