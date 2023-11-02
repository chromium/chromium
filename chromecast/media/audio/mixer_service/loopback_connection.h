// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_LOOPBACK_CONNECTION_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_LOOPBACK_CONNECTION_H_

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chromecast/media/audio/mixer_service/loopback_interrupt_reason.h"
#include "chromecast/media/audio/mixer_service/mixer_connection.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {
namespace mixer_service {
class Generic;

// Connection for observing loopback audio data from the mixer. Must be created
// and used on an IO thread.
class LoopbackConnection : public MixerConnection,
                           public MixerSocket::Delegate {
 public:
  // Observer for audio loopback data.
  class Delegate {
   public:
    // Called whenever loopback audio data is available. The |timestamp| is the
    // estimated time in microseconds (relative to the audio clock) that
    // the audio will actually be output. |length| is the length of the audio
    // |data| in bytes. The format of the data is given by |sample_format| and
    // |num_channels|.
    virtual void OnLoopbackAudio(int64_t timestamp,
                                 media::SampleFormat sample_format,
                                 int sample_rate,
                                 int num_channels,
                                 uint8_t* data,
                                 int length) = 0;

    // Called if the loopback data is not continuous (ie, does not accurately
    // represent the actual output) for any reason. For example, if there is an
    // output underflow, or if output is disabled due to no output streams.
    virtual void OnLoopbackInterrupted(LoopbackInterruptReason reason) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  explicit LoopbackConnection(Delegate* delegate);
  // For testing only.
  LoopbackConnection(Delegate* delegate,
                     std::unique_ptr<MixerSocket> connected_socket_for_test);

  LoopbackConnection(const LoopbackConnection&) = delete;
  LoopbackConnection& operator=(const LoopbackConnection&) = delete;

  ~LoopbackConnection() override;

  // Initiates connection to the mixer service. Delegate methods can be called
  // at any point after Connect() is called, until this is destroyed.
  void Connect();

 private:
  // MixerConnection implementation:
  void OnConnected(std::unique_ptr<MixerSocket> socket) override;
  void OnConnectionError() override;

  // MixerSocket::Delegate implementation:
  bool HandleMetadata(const Generic& message) override;
  bool HandleAudioData(char* data, size_t size, int64_t timestamp) override;

  Delegate* const delegate_;

  std::unique_ptr<MixerSocket> socket_;

  media::SampleFormat format_ = kUnknownSampleFormat;
  int sample_rate_ = 0;
  int num_channels_ = 0;
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_LOOPBACK_CONNECTION_H_
