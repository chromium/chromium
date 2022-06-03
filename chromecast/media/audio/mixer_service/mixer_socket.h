// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_

#include <cstdint>

#include "chromecast/media/audio/net/audio_socket.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace chromecast {
namespace media {
namespace mixer_service {
class Generic;

// AudioSocket implementation for sending and receiving messages to/from the
// mixer service.
class MixerSocket : public AudioSocket {
 public:
  class Delegate : public AudioSocket::Delegate {
   public:
    // Called when metadata is received from the other side of the connection.
    // Return |true| if the socket should continue to receive messages.
    virtual bool HandleMetadata(const Generic& message);

   protected:
    ~Delegate() override = default;
  };

  explicit MixerSocket(std::unique_ptr<net::StreamSocket> socket);
  MixerSocket();
  MixerSocket(const MixerSocket&) = delete;
  MixerSocket& operator=(const MixerSocket&) = delete;
  ~MixerSocket() override;

  // Sets/changes the delegate. Must be called immediately after creation
  // (ie, synchronously on the same sequence).
  void SetDelegate(Delegate* delegate);

 private:
  // AudioSocket implementation:
  bool ParseMetadata(char* data, size_t size) override;

  Delegate* delegate_ = nullptr;
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_
