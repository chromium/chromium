// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/cma/pipeline/load_type.h"
#include "chromecast/media/cma/pipeline/media_pipeline_client.h"
#include "media/base/time_delta_interpolator.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}  // namespace media

namespace chromecast {
namespace media {
class AudioPipelineImpl;
class BufferingController;
class CastCdmContext;
class CodedFrameProvider;
class VideoPipelineImpl;
struct AvPipelineClient;
struct VideoPipelineClient;

class MediaPipelineImpl {
 public:
  MediaPipelineImpl();

  MediaPipelineImpl(const MediaPipelineImpl&) = delete;
  MediaPipelineImpl& operator=(const MediaPipelineImpl&) = delete;

  ~MediaPipelineImpl();

  // Initialize the media pipeline: the pipeline is configured based on
  // |load_type|.
  void Initialize(LoadType load_type,
                  std::unique_ptr<CmaBackend> media_pipeline_backend,
                  bool is_buffering_enabled);

  void SetClient(MediaPipelineClient client);
  void SetCdm(const base::UnguessableToken* cdm_id);

  ::media::PipelineStatus InitializeAudio(
      const ::media::AudioDecoderConfig& config,
      AvPipelineClient client,
      std::unique_ptr<CodedFrameProvider> frame_provider);
  ::media::PipelineStatus InitializeVideo(
      const std::vector<::media::VideoDecoderConfig>& configs,
      VideoPipelineClient client,
      std::unique_ptr<CodedFrameProvider> frame_provider);
  void StartPlayingFrom(base::TimeDelta time);
  void Flush(base::OnceClosure flush_cb);
  void SetPlaybackRate(double playback_rate);
  void SetVolume(float volume);
  base::TimeDelta GetMediaTime() const;
  bool HasAudio() const;
  bool HasVideo() const;

  void SetCdm(CastCdmContext* cdm);

 private:
  enum BackendState {
    BACKEND_STATE_UNINITIALIZED,
    BACKEND_STATE_INITIALIZED,
    BACKEND_STATE_PLAYING,
    BACKEND_STATE_PAUSED
  };
  struct FlushTask;
  void CheckForPlaybackStall(base::TimeDelta media_time,
                             base::TimeTicks current_stc);

  void OnFlushDone(bool is_audio_stream);

  // Invoked to notify about a change of buffering state.
  void OnBufferingNotification(bool is_buffering);

  void UpdateMediaTime();
  void OnError(::media::PipelineStatus error);
  void ResetBitrateState();

  base::ThreadChecker thread_checker_;
  MediaPipelineClient client_;
  std::unique_ptr<BufferingController> buffering_controller_;
  CastCdmContext* cdm_context_;

  // Interface with the underlying hardware media pipeline.
  BackendState backend_state_;
  // Playback rate set by the upper layer.
  // Cached here because CMA pipeline backend does not support rate == 0,
  // which is emulated by pausing the backend.
  float playback_rate_;

  // Since av pipeline still need to access device components in their
  // destructor, it's important to delete them first.
  std::unique_ptr<CmaBackend> media_pipeline_backend_;
  CmaBackend::AudioDecoder* audio_decoder_;
  CmaBackend::VideoDecoder* video_decoder_;
  std::unique_ptr<AudioPipelineImpl> audio_pipeline_;
  std::unique_ptr<VideoPipelineImpl> video_pipeline_;
  std::unique_ptr<FlushTask> pending_flush_task_;

  // The media time is retrieved at regular intervals.
  bool pending_time_update_task_;
  base::TimeDelta start_media_time_;
  base::TimeDelta last_media_time_;

  // Used to make the statistics update period a multiplier of the time update
  // period.
  int statistics_rolling_counter_;
  base::TimeTicks last_sample_time_;
  base::TimeDelta elapsed_time_delta_;
  int audio_bytes_for_bitrate_estimation_;
  int video_bytes_for_bitrate_estimation_;

  // Playback stalled handling.
  bool playback_stalled_;
  base::TimeTicks playback_stalled_time_;
  bool playback_stalled_notification_sent_;

  // It's used to estimate current media time when the timestamp returned by
  // backend is invalid.
  ::media::TimeDeltaInterpolator media_time_interpolator_;

  bool waiting_for_first_have_enough_data_ = true;

  base::WeakPtr<MediaPipelineImpl> weak_this_;
  base::WeakPtrFactory<MediaPipelineImpl> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_IMPL_H_
