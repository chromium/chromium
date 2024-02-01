// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_CAST_AUDIO_DEMUXER_H_
#define CHROMECAST_MEDIA_API_CAST_AUDIO_DEMUXER_H_

#include <string_view>

#include "base/time/time.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {
class DecoderBufferBase;

// Demuxes an audio container and provides audio config and buffers that can be
// used to play the audio via the CMA backend. Must be created and destroyed on
// the same thread, and all methods must be called on that thread.
class CastAudioDemuxer {
 public:
  class Delegate {
   public:
    // Called with the audio config for the audio resource. Always called before
    // OnDemuxedAudioBuffer() and never called more than once.
    virtual void OnDemuxedAudioConfig(const AudioConfig& config) {}

    // Called for each buffer read from the audio resource. This may be called
    // multiple times, and the last call will pass an end-of-stream buffer
    // unless an error occurs.
    virtual void OnDemuxedAudioBuffer(scoped_refptr<DecoderBufferBase> buffer) {
    }

    // Called with the duration of the audio resource after the entire audio
    // resource has been read. No more methods will be called after this. It is
    // safe to destroy the CastAudioDemuxer in this callback.
    virtual void OnDemuxComplete(base::TimeDelta duration) = 0;

    // Called when the audio resource could not be read due to an error. No more
    // methods will be called after this. It is safe to destroy the
    // CastAudioDemuxer in this callback.
    virtual void OnDemuxError() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  virtual ~CastAudioDemuxer() = default;

  // Creates a CastAudioDemuxer instance for the given |audio_data|.
  // |audio_data| must outlive the demuxer.
  static std::unique_ptr<CastAudioDemuxer> Create(std::string_view audio_data,
                                                  Delegate* delegate);

  // Sets the base timestamp for the audio buffers passed to
  // OnDemuxedAudioBuffer() on the delegate so that the first buffer has a
  // timestamp equal to |timestamp| and the timestamps of all subsequent buffers
  // are adjusted accordingly. Must not be called after Demux().
  virtual void SetBaseTimestamp(base::TimeDelta timestamp) = 0;

  // Demuxes the audio data. May be called only once. Unless an error occurs,
  // this triggers a call to OnDemuxedAudioConfig() on the delegate followed by
  // one or more calls to OnDemuxedAudioBuffer() and finally a call to
  // OnDemuxComplete(), after which no more methods will be called. If an error
  // occurs, OnDemuxError() will be called and no more methods will be called.
  virtual void Demux() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_CAST_AUDIO_DEMUXER_H_
