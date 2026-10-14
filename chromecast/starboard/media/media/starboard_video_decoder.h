// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_VIDEO_DECODER_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_VIDEO_DECODER_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/starboard_decoder.h"

namespace chromecast {
namespace media {

// A VideoDecoder that sends buffers to a Starboard SbPlayer.
//
// All functions and the constructor/destructor must be called on the same
// sequence (the media thread).
class StarboardVideoDecoder
    : virtual public StarboardDecoder,
      virtual public MediaPipelineBackend::VideoDecoder {
 public:
  explicit StarboardVideoDecoder(StarboardApiWrapper* starboard);
  ~StarboardVideoDecoder() override;

  // Returns the video config or nullopt if the config has not been set yet.
  const std::optional<StarboardVideoSampleInfo>& GetVideoSampleInfo();

  // Snapshots `total_video_frames` and `dropped_video_frames` from `player`
  // before `player` is destroyed so `GetStatistics()` remains monotonic across
  // SbPlayer recreations, re-queues any in-flight write, and marks the decoder
  // uninitialized while preserving any in-band resolution change.
  void PrepareForPlayerRecreation(void* player);

  // MediaPipelineBackend::VideoDecoder implementation:
  bool SetConfig(const VideoConfig& config) override;
  void GetStatistics(Statistics* statistics) override;
  MediaPipelineBackend::BufferStatus PushBuffer(
      CastDecoderBuffer* buffer) override;
  void SetDelegate(Delegate* delegate) override;

  // StarboardDecoder implementation:
  std::optional<EncryptionScheme> GetEncryptionScheme() override;

 private:
  // StarboardDecoder implementation:
  void InitializeInternal() override;
  void StopInternal() override;

  SEQUENCE_CHECKER(sequence_checker_);
  void CheckForInBandResolutionChange(const CastDecoderBuffer& buffer);
  void UpdateResolution(int display_width,
                        int display_height,
                        int coded_width,
                        int coded_height,
                        const char* codec_name);

  // Since MIME type is passed as a c-string to starboard, we need to ensure
  // that the backing data does not go out of scope before starboard reads it.
  std::string codec_mime_;
  std::optional<StarboardVideoSampleInfo> video_sample_info_;
  // If true, this decoder should report the resolution the next time PushBuffer
  // is called.
  bool resolution_changed_ = false;
  VideoConfig config_;
  int container_width_ = 0;
  int container_height_ = 0;
  bool has_pending_pre_init_push_ = false;
  bool preserve_in_band_resolution_on_reinit_ = false;
  // The number of bytes sent to the SbPlayer. Used for statistics.
  uint64_t decoded_bytes_ = 0;
  uint64_t accumulated_decoded_frames_ = 0;
  uint64_t accumulated_dropped_frames_ = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_VIDEO_DECODER_H_
