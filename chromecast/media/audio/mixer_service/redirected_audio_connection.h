// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_REDIRECTED_AUDIO_CONNECTION_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_REDIRECTED_AUDIO_CONNECTION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chromecast/media/audio/mixer_service/mixer_connection.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {
namespace mixer_service {
class Generic;

// Connection for redirecting audio from the mixer to somewhere else. Must be
// created and used on an IO thread.
class RedirectedAudioConnection : public MixerConnection,
                                  public MixerSocket::Delegate {
 public:
  // Offset of start of audio data in an IOBuffer.
  static constexpr int kAudioDataOffset = MixerSocket::kAudioMessageHeaderSize;

  using StreamMatchPatterns = std::vector<
      std::pair<AudioContentType, std::string /* device ID pattern */>>;

  struct Config {
    // The number of output channels to send to the redirected output.
    int num_output_channels = 2;

    // Channel layout of the redirected audio, used for up/downmixing if needed.
    // Leave as UNSUPPORTED to autodetect based on |num_output_channels|.
    media::ChannelLayout output_channel_layout =
        media::ChannelLayout::UNSUPPORTED;

    // The order of this redirector (used to determine which output receives the
    // audio stream, if more than one redirection applies to a single stream).
    int order = 0;

    // Whether or not to apply the normal volume attenuation to the stream
    // that is being redirected.
    bool apply_volume = false;

    // Any extra delay to apply to the timestamps sent to the redirected output.
    // Note that the delayed timestamp will be used internally for AV sync.
    int64_t extra_delay_microseconds = 0;
  };

  // Observer for redirected audio data.
  class Delegate {
   public:
    // Called whenever redirected audio data is available. The |timestamp| is
    // the estimated time in microseconds (relative to the audio clock) that
    // the audio would have been output. |frames| is the number of frames of
    // audio data in |data|. The data is always in planar float format, with the
    // number of channels as specified in the config.
    virtual void OnRedirectedAudio(int64_t timestamp,
                                   float* data,
                                   int frames) = 0;

    // This will be called before OnRedirectedAudio is called for the first
    // time, and subsequently whenever the sample rate changes.
    virtual void SetSampleRate(int sample_rate) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  RedirectedAudioConnection(const Config& config, Delegate* delegate);

  RedirectedAudioConnection(const RedirectedAudioConnection&) = delete;
  RedirectedAudioConnection& operator=(const RedirectedAudioConnection&) =
      delete;

  ~RedirectedAudioConnection() override;

  // Sets the patterns which determine which audio streams should be redirected.
  // If a stream has the same content type and the device ID matches the
  // glob-style device ID pattern of any entry in this list, that stream will be
  // redirected.
  void SetStreamMatchPatterns(StreamMatchPatterns patterns);

  // Initiates connection to the mixer service. Delegate methods can be called
  // at any point after Connect() is called, until this is destroyed.
  void Connect();
  void ConnectForTest(std::unique_ptr<MixerSocket> connected_socket_for_test);

 private:
  // MixerConnection implementation:
  void OnConnected(std::unique_ptr<MixerSocket> socket) override;
  void OnConnectionError() override;

  // MixerSocket::Delegate implementation:
  bool HandleMetadata(const Generic& message) override;
  bool HandleAudioData(char* data, size_t size, int64_t timestamp) override;

  const Config config_;
  Delegate* const delegate_;

  StreamMatchPatterns stream_match_patterns_;

  std::unique_ptr<MixerSocket> socket_;

  int sample_rate_ = 0;
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_REDIRECTED_AUDIO_CONNECTION_H_
