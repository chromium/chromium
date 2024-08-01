// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_MEDIA_PIPELINE_BACKEND_STARBOARD_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_MEDIA_PIPELINE_BACKEND_STARBOARD_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/starboard_audio_decoder.h"
#include "chromecast/starboard/media/media/starboard_video_decoder.h"
#include "chromecast/starboard/media/media/starboard_video_plane.h"

namespace chromecast {
namespace media {

// A backend that uses starboard for decoding/rendering.
//
// Public functions and the destructor must be called on the sequence that
// consntructed the MediaPipelineBackendStarboard.
class MediaPipelineBackendStarboard : public MediaPipelineBackend {
 public:
  MediaPipelineBackendStarboard(const MediaPipelineDeviceParams& params,
                                StarboardVideoPlane* video_plane);
  ~MediaPipelineBackendStarboard() override;

  // For testing purposes, `starboard` will be used to call starboard functions.
  void TestOnlySetStarboardApiWrapper(
      std::unique_ptr<StarboardApiWrapper> starboard);

  // MediaPipelineBackend implementation:
  AudioDecoder* CreateAudioDecoder() override;
  VideoDecoder* CreateVideoDecoder() override;
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  int64_t GetCurrentPts() override;
  bool SetPlaybackRate(float rate) override;

  // StarboardVideoPlane::Delegate implementation:

 private:
  // Represents the state of the backend. See the documentation of
  // media_pipeline_backend.h for more information about valid transitions
  // between states.
  enum class State {
    kUninitialized,
    kInitialized,
    kPlaying,
    kPaused,
  };

  // Called when the video plane's geometry changes. This means that we need to
  // update the SbPlayer's bounds.
  //
  // Expects that `display_rect` is in display coordinates, NOT (physical)
  // screen coordinates.
  //
  // This function must be called on media_task_runner_.
  void OnGeometryChanged(const RectF& display_rect,
                         StarboardVideoPlane::Transform transform);

  // Creates the starboard player object.
  void CreatePlayer();

  // Called when a starboard audio or video decoder needs more samples. This
  // occurs after either:
  // 1. A sample has been decoded, or
  // 2. A seek has finished
  //
  // For case 1, we notify the relevant decoder that the sample has been
  // decoded. For case 2, we re-initialize the decoder, meaning it will start
  // pushing buffers to starboard again.
  void OnSampleDecoded(void* player,
                       StarboardMediaType type,
                       StarboardDecoderState decoder_state,
                       int ticket);

  // Called when a sample can be deallocated. The decoders handle the actual
  // deallocation.
  void DeallocateSample(void* player, const void* sample_buffer);

  // Called when the SbPlayer's state has changed.
  void OnPlayerStatus(void* player, StarboardPlayerState state, int ticket);

  // Called when the SbPlayer has encountered an error.
  void OnPlayerError(void* player,
                     StarboardPlayerError error,
                     const std::string& message);

  // Called when starboard has started. This must be called on the media task
  // runner.
  void OnStarboardStarted();

  // Performs the actual play logic by setting the playback rate to a non-zero
  // value. player_ must not be null when this is called.
  void DoPlay();

  // Performs the actual pause logic by setting the playback rate to 0. player_
  // must not be null when this is called.
  void DoPause();

  // Seeks to last_seek_pts_ and increments seek_ticket_.
  void DoSeek();

  // A pure function that calls OnSampleDecoded. `context` is a pointer to a
  // MediaPipelineBackendStarboard object. The rest of the arguments are
  // forwarded to OnSampleDecoded.
  static void CallOnSampleDecoded(void* player,
                                  void* context,
                                  StarboardMediaType type,
                                  StarboardDecoderState decoder_state,
                                  int ticket);

  // Called by starboard when a sample can be deallocated. Note that this may be
  // called after CallOnSampleDecoded is called for a given sample, meaning that
  // the sample needs to outlive the call to CallOnSampleDecoded.
  static void CallDeallocateSample(void* player,
                                   void* context,
                                   const void* sample_buffer);

  // Called by starboard when the player's status changes. Notably, this will be
  // called with state kStarboardPlayerStateEndOfStream when the end of stream
  // buffer has been rendered.
  static void CallOnPlayerStatus(void* player,
                                 void* context,
                                 StarboardPlayerState state,
                                 int ticket);

  // Called by starboard when there is a player error.
  static void CallOnPlayerError(void* player,
                                void* context,
                                StarboardPlayerError error,
                                const char* message);

  StarboardPlayerCallbackHandler player_callback_handler_ = {
      /*context=*/this,      &CallOnSampleDecoded,
      &CallDeallocateSample, &CallOnPlayerStatus,
      &CallOnPlayerError,
  };
  // Calls to Starboard are made through this struct, to allow tests to mock
  // their behavior (and not rely on Starboard).
  std::unique_ptr<StarboardApiWrapper> starboard_;
  State state_ = State::kUninitialized;
  float playback_rate_ = 1.0;
  int64_t last_seek_pts_ = -1;
  // Ticket representing the last seek. This is necessary to recognize callbacks
  // from old seeks, which are no longer relevant.
  int seek_ticket_ = 0;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  // An opaque handle to the SbPlayer.
  void* player_ = nullptr;
  std::optional<StarboardAudioDecoder> audio_decoder_;
  std::optional<StarboardVideoDecoder> video_decoder_;
  std::optional<RectF> pending_geometry_change_;
  StarboardVideoPlane* video_plane_ = nullptr;
  int64_t video_plane_callback_token_ = 0;
  // If true, we should render frames immediately to minimize latency. This
  // should be true when mirroring.
  bool is_streaming_ = false;

  // This must be destructed first.
  base::WeakPtrFactory<MediaPipelineBackendStarboard> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_MEDIA_PIPELINE_BACKEND_STARBOARD_H_
