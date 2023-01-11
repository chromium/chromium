// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_CONNECTION_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_CONNECTION_H_

#include <atomic>
#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/audio/net/common.pb.h"
#include "chromecast/media/audio/playback_rate_shifter.h"
#include "chromecast/media/cma/backend/mixer/mixer_input.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"
#include "media/base/channel_layout.h"
#include "media/base/media_util.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {
class AudioBus;
}  // namespace media

namespace net {
class IOBuffer;
}  // namespace net

namespace chromecast {
class IOBufferPool;

namespace media {
class RateAdjuster;
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
                             public MixerInput::Source {
 public:
  using RenderingDelay = MediaPipelineBackend::AudioDecoder::RenderingDelay;

  MixerInputConnection(StreamMixer* mixer,
                       std::unique_ptr<mixer_service::MixerSocket> socket,
                       const mixer_service::OutputStreamParams& params);

  MixerInputConnection(const MixerInputConnection&) = delete;
  MixerInputConnection& operator=(const MixerInputConnection&) = delete;

  // Only public to allow task_runner->DeleteSoon() to work.
  ~MixerInputConnection() override;

 private:
  friend class MixerServiceReceiver;
  class TimestampedFader;

  enum class State {
    kUninitialized,   // Not initialized by the mixer yet.
    kNormalPlayback,  // Normal playback.
    kGotEos,          // Got the end-of-stream buffer (normal playback).
    kSignaledEos,     // Sent EOS signal up to delegate.
    kRemoved,         // The caller has removed this source; finish playing out.
  };

  // mixer_service::MixerSocket::Delegate implementation:
  bool HandleMetadata(const mixer_service::Generic& message) override;
  bool HandleAudioData(char* data, size_t size, int64_t timestamp) override;
  bool HandleAudioBuffer(scoped_refptr<net::IOBuffer> buffer,
                         char* data,
                         size_t size,
                         int64_t timestamp) override;
  void OnConnectionError() override;

  void CreateBufferPool(int frame_count);
  void OnInactivityTimeout();
  void RestartPlaybackAt(int64_t timestamp, int64_t pts);
  void SetMediaPlaybackRate(double rate);
  void SetMediaPlaybackRateLocked(double rate) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void SetAudioClockRate(double rate);
  double ChangeAudioRate(double desired_clock_rate,
                         double error_slope,
                         double current_error) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void AdjustTimestamps(int64_t timestamp_adjustment);
  void SetPaused(bool paused);

  // MixerInput::Source implementation:
  size_t num_channels() const override;
  ::media::ChannelLayout channel_layout() const override;
  int sample_rate() const override;
  bool primary() override;
  const std::string& device_id() override;
  AudioContentType content_type() override;
  AudioContentType focus_type() override;
  int desired_read_size() override;
  int playout_channel() override;
  bool active() override;
  bool require_clock_rate_simulation() const override;

  void InitializeAudioPlayback(int read_size,
                               RenderingDelay initial_rendering_delay) override;
  int FillAudioPlaybackFrames(int num_frames,
                              RenderingDelay rendering_delay,
                              ::media::AudioBus* buffer) override;
  void OnAudioPlaybackError(MixerError error) override;
  void OnOutputUnderrun() override;
  void FinalizeAudioPlayback() override;

  int FillAudio(int num_frames,
                int64_t expected_playout_time,
                float* const* channels,
                bool after_silence) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  int FillTimestampedAudio(int num_frames,
                           int64_t expected_playout_time,
                           float* const* channels,
                           bool after_silence) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  int FillFromQueue(int num_frames, float* const* channels, int write_offset)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void LogUnderrun(int num_frames, int filled) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void WritePcm(scoped_refptr<net::IOBuffer> data);
  RenderingDelay QueueData(scoped_refptr<net::IOBuffer> data)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  double ExtraDelayFrames() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void RemoveSelf() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void PostPcmCompletion();
  void PostEos();
  void PostError(MixerError error);
  void PostStreamUnderrun();
  void PostOutputUnderrun();
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
  const ::media::ChannelLayout channel_layout_;
  const int input_samples_per_second_;
  const audio_service::SampleFormat sample_format_;
  const bool primary_;
  const std::string device_id_;
  const AudioContentType content_type_;
  const AudioContentType focus_type_;
  const int playout_channel_;
  const bool pts_is_timestamp_;
  const int64_t max_timestamp_error_;
  const bool never_crop_;
  const bool enable_audio_clock_simulation_;

  std::atomic<int> effective_playout_channel_;

  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  int max_queued_frames_;
  // Minimum number of frames buffered before starting to fill data.
  int start_threshold_frames_;
  int min_start_threshold_;

  scoped_refptr<IOBufferPool> buffer_pool_;

  // Only used on the IO thread.
  bool audio_ready_for_playback_fired_ = false;
  bool never_timeout_connection_ = false;
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
  RenderingDelay next_delay_ GUARDED_BY(lock_);
  int mixer_read_size_ GUARDED_BY(lock_) = 0;
  int current_buffer_offset_ GUARDED_BY(lock_) = 0;
  std::unique_ptr<RateAdjuster> rate_adjuster_ GUARDED_BY(lock_);
  int64_t total_filled_frames_ GUARDED_BY(lock_) = 0;
  bool filled_some_since_resume_ GUARDED_BY(lock_) = false;
  const int fade_frames_;
  std::unique_ptr<TimestampedFader> timestamped_fader_ GUARDED_BY(lock_);
  PlaybackRateShifter rate_shifter_ GUARDED_BY(lock_);
  bool in_underrun_ GUARDED_BY(lock_) = false;
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
  bool fed_one_silence_buffer_after_removal_ GUARDED_BY(lock_) = false;
  bool removed_self_ GUARDED_BY(lock_) = false;

  base::RepeatingClosure pcm_completion_task_;
  base::RepeatingClosure eos_task_;
  base::RepeatingClosure ready_for_playback_task_;
  base::RepeatingClosure post_stream_underrun_task_;

  base::WeakPtr<MixerInputConnection> weak_this_;
  base::WeakPtrFactory<MixerInputConnection> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_CONNECTION_H_
