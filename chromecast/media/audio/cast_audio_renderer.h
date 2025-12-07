// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_RENDERER_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_RENDERER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/common/mojom/audio_socket.mojom.h"
#include "chromecast/media/audio/audio_output_service/output_stream_connection.h"
#include "media/base/audio_renderer.h"
#include "media/base/buffering_state.h"
#include "media/base/demuxer_stream.h"
#include "media/base/time_source.h"
#include "media/base/waiting.h"
#include "media/mojo/mojom/cast_application_media_info_manager.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {
class BrowserInterfaceBrokerProxy;
}  // namespace blink

namespace media {
class AudioDecoderConfig;
class DecoderBuffer;
class DecryptingDemuxerStream;
class MediaLog;
}  // namespace media

namespace chromecast {
namespace media {

// Cast implementation of ::media::AudioRenderer which sends decrypted audio
// buffers to the audio output service for decoding and rendering.
// Threading model: after constructed, ::media::AudioRenderer methods will be
// called on the main thread. ::media::TimeSource methods will be called from
// multiple threads (so lock is required for timeline related fields/methods).
// audio_output_service::OutputStreamConnection::Delegate methods will be called
// from an IO thread.
class CastAudioRenderer
    : public ::media::AudioRenderer,
      public ::media::TimeSource,
      public audio_output_service::OutputStreamConnection::Delegate {
 public:
  CastAudioRenderer(scoped_refptr<base::SequencedTaskRunner> media_task_runner,
                    ::media::MediaLog* media_log,
                    blink::BrowserInterfaceBrokerProxy* interface_broker);
  CastAudioRenderer(const CastAudioRenderer&) = delete;
  CastAudioRenderer& operator=(const CastAudioRenderer&) = delete;
  ~CastAudioRenderer() override;

 private:
  enum class PlaybackState {
    kStopped,

    // We've called Start(), but haven't received updated state. |start_time_|
    // should not be used yet.
    kStarting,

    // Playback is active. When the stream reaches EOS it stays in the kPlaying
    // state.
    kPlaying,
  };

  // ::media::AudioRenderer implementation:
  void Initialize(::media::DemuxerStream* stream,
                  ::media::CdmContext* cdm_context,
                  ::media::RendererClient* client,
                  ::media::PipelineStatusCallback init_cb) override;
  ::media::TimeSource* GetTimeSource() override;
  void Flush(base::OnceClosure callback) override;
  void StartPlaying() override;
  void SetVolume(float volume) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void SetPreservesPitch(bool preserves_pitch) override;
  void SetWasPlayedWithUserActivationAndHighMediaEngagement(
      bool was_played_with_user_activation_and_high_media_engagement) override;

  // ::media::TimeSource implementation:
  void StartTicking() override;
  void StopTicking() override;
  void SetPlaybackRate(double playback_rate) override;
  void SetMediaTime(base::TimeDelta time) override;
  base::TimeDelta CurrentMediaTime() override;
  bool GetWallClockTimes(
      const std::vector<base::TimeDelta>& media_timestamps,
      std::vector<base::TimeTicks>* wall_clock_times) override;

  // audio_output_service::OutputStreamConnection::Delegate implementation:
  void OnBackendInitialized(
      const audio_output_service::BackendInitializationStatus& status) override;
  void OnNextBuffer(int64_t media_timestamp_microseconds,
                    int64_t reference_timestamp_microseconds,
                    int64_t delay_microseconds,
                    int64_t delay_timestamp_microseconds) override;

  void ScheduleFetchNextBuffer();
  void FetchNextBuffer();
  void OnEndOfStream();

  // Returns current PlaybackState.
  PlaybackState GetPlaybackState() EXCLUSIVE_LOCKS_REQUIRED(timeline_lock_);

  // Helper function used with DCHECKs to hold |timeline_lock_|.
  bool CurrentPlaybackStateEquals(PlaybackState playback_state);

  // Used to update |state_|.
  void SetPlaybackState(PlaybackState state)
      EXCLUSIVE_LOCKS_REQUIRED(timeline_lock_);

  // Returns true if media clock is ticking and the rate is above 0.0.
  bool IsTimeMoving() EXCLUSIVE_LOCKS_REQUIRED(timeline_lock_);

  // Updates TimelineFunction parameters after StopTicking() or
  // SetPlaybackRate(0.0). Normally these parameters are provided by
  // the audio output service, but this happens asynchronously and we need to
  // make sure that StopTicking() and SetPlaybackRate(0.0) stop the media clock
  // synchronously. Must be called before updating the |state_|.
  void UpdateTimelineOnStop() EXCLUSIVE_LOCKS_REQUIRED(timeline_lock_);

  void UpdateAudioDecoderConfig(const ::media::AudioDecoderConfig& config);

  base::TimeDelta CurrentMediaTimeLocked()
      EXCLUSIVE_LOCKS_REQUIRED(timeline_lock_);

  // Updates buffer state and notifies the |client_| if necessary.
  void SetBufferState(::media::BufferingState buffer_state);

  void OnNewBuffersRead(
      ::media::DemuxerStream::Status status,
      ::media::DemuxerStream::DecoderBufferVector buffers_queue);
  void OnNewBuffer(::media::DemuxerStream::Status read_status,
                   scoped_refptr<::media::DecoderBuffer> buffer);
  void OnError(::media::PipelineStatus pipeline_status);
  void OnDecryptingDemuxerInitialized(::media::PipelineStatus pipeline_status);
  void OnWaiting(::media::WaitingReason reason);
  void OnApplicationMediaInfoReceived(
      ::media::mojom::CastApplicationMediaInfoPtr application_media_info);
  void FlushInternal();

  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  ::media::MediaLog* const media_log_;

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  ::media::DemuxerStream* demuxer_stream_ = nullptr;
  std::unique_ptr<::media::DecryptingDemuxerStream> decrypting_stream_;
  ::media::RendererClient* renderer_client_ = nullptr;
  base::SequenceBound<audio_output_service::OutputStreamConnection>
      output_connection_;

  bool is_pending_demuxer_read_ = false;
  ::media::PipelineStatusCallback init_cb_;
  base::OnceClosure flush_cb_;

  float volume_ = 1.0f;

  ::media::BufferingState buffer_state_ = ::media::BUFFERING_HAVE_NOTHING;

  base::TimeDelta last_pushed_timestamp_ = base::TimeDelta::Min();
  base::OneShotTimer read_timer_;

  // Indicates that StartPlaying() has been called. Note that playback doesn't
  // start until StartTicking() is called.
  bool renderer_started_ = false;

  // Indicates that StartTicking() has been called.
  bool ticking_ = false;

  bool is_at_end_of_stream_ = false;

  // TimeSource interface is not single-threaded. The lock is used to guard
  // fields that are accessed in the TimeSource implementation.
  base::Lock timeline_lock_;

  float playback_rate_ GUARDED_BY(timeline_lock_) = 0.0f;

  // Should be changed by calling SetPlaybackState() on the main thread.
  PlaybackState state_ GUARDED_BY(timeline_lock_) = PlaybackState::kStopped;

  base::TimeTicks reference_time_ GUARDED_BY(timeline_lock_);
  base::TimeDelta media_pos_ GUARDED_BY(timeline_lock_);

  // TODO(b/173250111): update these values based on whether the media session
  // is multiroom.
  base::TimeDelta min_lead_time_ = base::Milliseconds(100);
  base::TimeDelta max_lead_time_ = base::Milliseconds(500);

  mojo::PendingRemote<::media::mojom::CastApplicationMediaInfoManager>
      application_media_info_manager_pending_remote_;
  mojo::Remote<::media::mojom::CastApplicationMediaInfoManager>
      application_media_info_manager_remote_;
  mojo::PendingRemote<mojom::AudioSocketBroker>
      audio_socket_broker_pending_remote_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CastAudioRenderer> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_RENDERER_H_
