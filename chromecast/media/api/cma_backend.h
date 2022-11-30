// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_CMA_BACKEND_H_
#define CHROMECAST_MEDIA_API_CMA_BACKEND_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

// Interface for the media backend used by the CMA pipeline. The implementation
// is selected by CmaBackendFactory. MediaPipelineBackend is a lower-level
// interface used to abstract the platform, with a separate implementation for
// each platform, while CmaBackend implementations are used across multiple
// platforms.
class CmaBackend {
 public:
  using BufferStatus = MediaPipelineBackend::BufferStatus;

  class Decoder {
   public:
    using BufferStatus = MediaPipelineBackend::BufferStatus;
    using Delegate = MediaPipelineBackend::Decoder::Delegate;

    // These methods have the same behavior as the corresponding methods on
    // MediaPipelineBackend::Decoder.
    // See chromecast/public/media/media_pipeline_backend.h for documentation.
    virtual void SetDelegate(Delegate* delegate) = 0;

    // Pushes buffer to decoder.  A new |buffer| should be used for each call
    // and should not be mutated by the caller.
    virtual BufferStatus PushBuffer(
        scoped_refptr<DecoderBufferBase> buffer) = 0;

   protected:
    virtual ~Decoder() = default;
  };

  class AudioDecoder : public Decoder {
   public:
    using RenderingDelay = MediaPipelineBackend::AudioDecoder::RenderingDelay;
    using Statistics = MediaPipelineBackend::AudioDecoder::Statistics;
    using AudioTrackTimestamp =
        MediaPipelineBackend::AudioDecoder::AudioTrackTimestamp;

    // These methods have the same behavior as the corresponding methods on
    // MediaPipelineBackend::AudioDecoder.
    // See chromecast/public/media/media_pipeline_backend.h for documentation.
    virtual bool SetConfig(const AudioConfig& config) = 0;
    virtual bool SetVolume(float multiplier) = 0;
    virtual RenderingDelay GetRenderingDelay() = 0;
    virtual void GetStatistics(Statistics* statistics) = 0;
    virtual AudioTrackTimestamp GetAudioTrackTimestamp() = 0;
    virtual int GetStartThresholdInFrames() = 0;

    // Returns true if the audio decoder requires that encrypted buffers be
    // decrypted before being passed to PushBuffer(). The return value may
    // change whenever SetConfig() is called or the backend is initialized.
    virtual bool RequiresDecryption() = 0;

   protected:
    ~AudioDecoder() override = default;
  };

  class VideoDecoder : public Decoder {
   public:
    using Statistics = MediaPipelineBackend::VideoDecoder::Statistics;

    // These methods have the same behavior as the corresponding methods on
    // MediaPipelineBackend::VideoDecoder.
    // See chromecast/public/media/media_pipeline_backend.h for documentation.
    virtual bool SetConfig(const VideoConfig& config) = 0;
    virtual void GetStatistics(Statistics* statistics) = 0;

   protected:
    ~VideoDecoder() override = default;
  };

  virtual ~CmaBackend() = default;

  // These methods have the same behavior as the corresponding methods on
  // MediaPipelineBackend. See chromecast/public/media/media_pipeline_backend.h
  // for documentation.
  virtual AudioDecoder* CreateAudioDecoder() = 0;
  virtual VideoDecoder* CreateVideoDecoder() = 0;
  virtual bool Initialize() = 0;
  virtual bool Start(int64_t start_pts) = 0;
  virtual void Stop() = 0;
  virtual bool Pause() = 0;
  virtual bool Resume() = 0;
  virtual int64_t GetCurrentPts() = 0;
  virtual bool SetPlaybackRate(float rate) = 0;

  // Logically pauses/resumes a backend instance, without actually pausing or
  // resuming it. This is used by multiroom output to avoid playback stutter on
  // resume.
  virtual void LogicalPause() = 0;
  virtual void LogicalResume() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_CMA_BACKEND_H_
