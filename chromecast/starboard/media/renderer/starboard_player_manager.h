// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_RENDERER_STARBOARD_PLAYER_MANAGER_H_
#define CHROMECAST_STARBOARD_MEDIA_RENDERER_STARBOARD_PLAYER_MANAGER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "chromecast/starboard/media/cdm/starboard_drm_wrapper.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/renderer/client_stats_tracker.h"
#include "chromecast/starboard/media/renderer/demuxer_stream_reader.h"
#include "chromecast/starboard/media/renderer/starboard_buffering_tracker.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/renderer_client.h"

namespace chromecast {

namespace metrics {
class CastMetricsHelper;
}  // namespace metrics

namespace media {

// Manages interactions with an SbPlayer. In particular, this class has several
// responsibilities:
//
// * Reading buffers from DemuxerStreams (this logic is encapsulated in a
//    separate class, DemuxerStreamReader, which is used by this one)
// * Providing those buffers to Starboard as they are requested
// * Deallocating those buffers once they are no longer needed
// * Notifying RendererClient when certain events occur
// * Updating RendererClient stats (this logic is encapsulated in a separate
//    class, ClientStatsTracker, which is used by this one)
//
// Instances of StarboardPlayerManager should be created via Create().
//
// Public functions must be called on the media sequence matching the runner
// which is provided to Create.
class StarboardPlayerManager {
 public:
  // Factory function for creating a StarboardPlayerManager. One of
  // `audio_stream` or `video_stream` may be null (but not both). All other args
  // must not be null.
  //
  // If an SbPlayer cannot be created, or if any of the conditions above are not
  // met, null will be returned.
  static std::unique_ptr<StarboardPlayerManager> Create(
      StarboardApiWrapper* starboard,
      ::media::DemuxerStream* audio_stream,
      ::media::DemuxerStream* video_stream,
      ::media::RendererClient* client,
      chromecast::metrics::CastMetricsHelper* cast_metrics_helper,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      bool enable_buffering);

  // Disallow copy and assign.
  StarboardPlayerManager(const StarboardPlayerManager&) = delete;
  StarboardPlayerManager& operator=(const StarboardPlayerManager&) = delete;

  ~StarboardPlayerManager();

  // Begins playback from `time`. This triggers a seek in starboard, which will
  // cause starboard to start requesting buffers. A request for buffers from
  // starboard in turn triggers a call to DemuxerStream::Read via
  // DemuxerStreamReader.
  void StartPlayingFrom(base::TimeDelta time);

  // Discards any pending buffers and pauses playback.
  void Flush();

  // Sets the playback rate. 0 means that playback is paused.
  void SetPlaybackRate(double playback_rate);

  // Sets the media volume. This is different from the system volume; it is
  // essentially a multiplier, e.g. for fade in/out effects. Most cast apps do
  // not use this.
  void SetVolume(float volume);

  // Returns the current media time.
  base::TimeDelta GetMediaTime();

  // Returns the SbPlayer owned by this object.
  void* GetSbPlayer();

 private:
  explicit StarboardPlayerManager(
      std::optional<StarboardDrmWrapper::DrmSystemResource> drm_resource,
      StarboardApiWrapper* starboard,
      ::media::DemuxerStream* audio_stream,
      ::media::DemuxerStream* video_stream,
      std::optional<StarboardAudioSampleInfo> audio_sample_info,
      std::optional<StarboardVideoSampleInfo> video_sample_info,
      ::media::RendererClient* client,
      chromecast::metrics::CastMetricsHelper* cast_metrics_helper,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner);

  // Pushes `buffer` to starboard.
  void PushBuffer(int seek_ticket,
                  StarboardSampleInfo sample_info,
                  scoped_refptr<::media::DecoderBuffer> buffer);

  // Signals to starboard that the end of a stream has been reached (for type
  // `type`).
  void PushEos(int seek_ticket, StarboardMediaType type);

  // Updates the client's stats based on an audio/video buffer being pushed.
  void UpdateStats(const StarboardSampleInfo& sample_info);

  // Updates cast metrics once playback begins (after a period of buffering).
  // This is a helper function; it is not called directly by Starboard.
  void UpdateMetricsOnPresenting();

  // Called by Starboard when a decoder's status changes.
  void OnDecoderStatus(void* player,
                       StarboardMediaType type,
                       StarboardDecoderState decoder_state,
                       int ticket);

  // Called by Starboard when a buffer can safely be deallocated.
  void DeallocateSample(void* player, const void* sample_buffer);

  // Called by Starboard when the player's state changes.
  void OnPlayerStatus(void* player, StarboardPlayerState state, int ticket);

  // Called by Starboard when a player-related error has occurred.
  void OnPlayerError(void* player,
                     StarboardPlayerError error,
                     std::string message);

  // Callbacks called by Starboard. These simply call the private methods
  // declared above.
  static void CallOnDecoderStatus(void* player,
                                  void* context,
                                  StarboardMediaType type,
                                  StarboardDecoderState decoder_state,
                                  int ticket);
  static void CallDeallocateSample(void* player,
                                   void* context,
                                   const void* sample_buffer);
  static void CallOnPlayerStatus(void* player,
                                 void* context,
                                 StarboardPlayerState state,
                                 int ticket);
  static void CallOnPlayerError(void* player,
                                void* context,
                                StarboardPlayerError error,
                                std::string message);

  StarboardPlayerCallbackHandler callback_handler_{
      this,
      &StarboardPlayerManager::CallOnDecoderStatus,
      &StarboardPlayerManager::CallDeallocateSample,
      &StarboardPlayerManager::CallOnPlayerStatus,
      &StarboardPlayerManager::CallOnPlayerError,
  };
  // Ensure that the underlying SbDrmSystem is not destructed until after this
  // class's destructor runs (so we can destroy SbPlayer first). This is
  // optional because it is only needed for DRM playback.
  std::optional<StarboardDrmWrapper::DrmSystemResource> drm_resource_;
  raw_ptr<StarboardApiWrapper> starboard_ = nullptr;
  // This class owns the SbPlayer.
  raw_ptr<void> player_ = nullptr;
  raw_ptr<::media::RendererClient> client_ = nullptr;
  ClientStatsTracker stats_tracker_;
  bool flushing_ = false;
  double playback_rate_ = 0.0;
  // Buffers from an old seek ticket can be safely ignored.
  int seek_ticket_ = 0;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  DemuxerStreamReader demuxer_stream_reader_;
  // Maps from a buffer address to the scoped_refptr managing the buffer's
  // lifetime.
  base::flat_map<raw_ptr<const void>, scoped_refptr<::media::DecoderBuffer>>
      addr_to_buffer_;
  std::optional<StarboardBufferingTracker> buffering_tracker_;
  raw_ptr<chromecast::metrics::CastMetricsHelper> cast_metrics_helper_ =
      nullptr;

  // This should be destructed first, to invalidate any weak ptrs.
  base::WeakPtrFactory<StarboardPlayerManager> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_RENDERER_STARBOARD_PLAYER_MANAGER_H_
