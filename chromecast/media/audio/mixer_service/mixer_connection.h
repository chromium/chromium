// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_CONNECTION_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_CONNECTION_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace chromecast {
namespace media {
namespace mixer_service {
class MixerSocket;

// Base class for connecting to the mixer service.
class MixerConnection {
 public:
  MixerConnection();

  MixerConnection(const MixerConnection&) = delete;
  MixerConnection& operator=(const MixerConnection&) = delete;

  virtual ~MixerConnection();

  // Initiates connection to the mixer service. Will call OnConnected() when
  // connection is successful.
  void Connect();

 protected:
  // Called when a connection is established to the mixer service.
  virtual void OnConnected(std::unique_ptr<MixerSocket> socket) = 0;

 private:
  void ConnectCallback(int result);
  void ConnectTimeout();

  std::unique_ptr<net::StreamSocket> connecting_socket_;
  base::OneShotTimer connection_timeout_;

  bool log_connection_failure_ = true;
  bool log_timeout_ = true;

  base::WeakPtrFactory<MixerConnection> weak_factory_;
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_CONNECTION_H_
