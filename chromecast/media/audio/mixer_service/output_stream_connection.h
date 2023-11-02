// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_OUTPUT_STREAM_CONNECTION_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_OUTPUT_STREAM_CONNECTION_H_

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chromecast/media/audio/mixer_service/mixer_connection.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "net/base/io_buffer.h"

namespace chromecast {
class CastEventBuilder;
class IOBufferPool;

namespace media {
namespace mixer_service {
class Generic;
class OutputStreamParams;

// Mixer service connection for an audio output stream. Not thread-safe; all
// usage of a given instance must be on the same sequence.
class OutputStreamConnection : public MixerConnection,
                               public MixerSocket::Delegate {
 public:
  class Delegate {
   public:
    // Keep in sync with mixer_service.proto:MixerUnderrun.Type
    enum class MixerUnderrunType {
      // An underrun was detected on mixer input.
      kStream = 0,
      // An underrun was detected on mixer output.
      kMixer = 1,
    };

    // Called to fill more audio data. The implementation should write up to
    // |frames| frames of audio data into |buffer|, and then call
    // SendNextBuffer() with the actual number of frames that were filled (or
    // 0 to indicate end-of-stream). The |playout_timestamp| indicates the
    // audio clock timestamp in microseconds when the first frame of the filled
    // data is expected to play out.
    virtual void FillNextBuffer(void* buffer,
                                int frames,
                                int64_t delay_timestamp,
                                int64_t delay) = 0;

    // Called when audio is ready to begin playing out, ie the start threshold
    // has been reached. |mixer_delay| is the delay before the first buffered
    // audio will start playing out, in microseconds.
    virtual void OnAudioReadyForPlayback(int64_t mixer_delay) {}

    // Called when the end of the stream has been played out. At this point it
    // is safe to delete the delegate without dropping any audio.
    virtual void OnEosPlayed() = 0;

    // Called when a mixer error has occurred; audio from this stream will no
    // longer be played out.
    virtual void OnMixerError() {}

    // Called when an underrun happens on mixer input/output.
    virtual void OnMixerUnderrun(MixerUnderrunType type) {}

    // Called when OutputStreamConnection records a cast event. It allows
    // the Delegate to provide some extra data to the event.
    virtual void ProcessCastEvent(CastEventBuilder* event) {}

   protected:
    virtual ~Delegate() = default;
  };

  OutputStreamConnection(Delegate* delegate, const OutputStreamParams& params);

  OutputStreamConnection(const OutputStreamConnection&) = delete;
  OutputStreamConnection& operator=(const OutputStreamConnection&) = delete;

  ~OutputStreamConnection() override;

  // Connects to the mixer. After this is called, delegate methods may start
  // to be called. If the mixer connection is lost, this will automatically
  // reconnect.
  void Connect();

  // Sends filled audio data (written into the buffer provided to
  // FillNextBuffer()) to the mixer. |filled_frames| indicates the number of
  // frames of audio that were actually written into the buffer, or 0 to
  // indicate end-of-stream. |pts| is the PTS of the first frame of filled
  // audio; this is only meaningful when params.use_start_timestamp is |true|.
  void SendNextBuffer(int filled_frames, int64_t pts = 0);

  // Sends a preallocated audio buffer. The buffer does not need to be prepared
  // first using MixerSocket::PrepareAudioBuffer(), since that is done inside
  // this method.
  void SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                       int filled_frames,
                       int64_t pts);

  // Sets the volume multiplier for this audio stream.
  void SetVolumeMultiplier(float multiplier);

  // Indicates that playback should (re)start playing PTS |pts| at time
  // |start_timestamp| in microseconds relative to the audio clock.
  void SetStartTimestamp(int64_t start_timestamp, int64_t pts);

  // Informs the mixer how fast the PTS increases per frame. For example if the
  // playback rate is 2.0, then each frame increases the PTS by
  // 2.0 / sample_rate seconds.
  void SetPlaybackRate(float playback_rate);

  // Changes the audio output clock rate. If the provided |rate| is outside of
  // the supported range, the rate will be clamped to the supported range.
  void SetAudioClockRate(double rate);

  // Pauses playback.
  void Pause();

  // Resumes playback.
  void Resume();

  // Adjusts timestamps.
  void SendTimestampAdjustment(int64_t timestamp_adjustment);

 private:
  // MixerConnection implementation:
  void OnConnected(std::unique_ptr<MixerSocket> socket) override;
  void OnConnectionError() override;

  // MixerSocket::Delegate implementation:
  bool HandleMetadata(const Generic& message) override;

  Delegate* const delegate_;
  std::unique_ptr<OutputStreamParams> params_;

  const int frame_size_;
  const int fill_size_frames_;

  const scoped_refptr<IOBufferPool> buffer_pool_;
  scoped_refptr<net::IOBuffer> audio_buffer_;

  std::unique_ptr<MixerSocket> socket_;
  float volume_multiplier_ = 1.0f;

  int64_t start_timestamp_ = INT64_MIN;
  int64_t start_pts_ = INT64_MIN;

  float playback_rate_ = 1.0f;
  double audio_clock_rate_ = 1.0;

  bool paused_ = false;
  bool sent_eos_ = false;

  bool dropping_audio_ = false;
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_OUTPUT_STREAM_CONNECTION_H_
