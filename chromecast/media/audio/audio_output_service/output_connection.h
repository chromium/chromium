// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_OUTPUT_CONNECTION_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_OUTPUT_CONNECTION_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace chromecast {
namespace media {
namespace audio_output_service {
class OutputSocket;

// Base class for connecting to the audio output service.
class OutputConnection {
 public:
  OutputConnection();
  OutputConnection(const OutputConnection&) = delete;
  OutputConnection& operator=(const OutputConnection&) = delete;
  virtual ~OutputConnection();

  // Initiates connection to the audio output service. Will call OnConnected()
  // when connection is successful.
  void Connect();

 protected:
  // Called when a connection is established to the audio output service.
  virtual void OnConnected(std::unique_ptr<OutputSocket> socket) = 0;

 private:
  void ConnectCallback(int result);
  void ConnectTimeout();

  std::unique_ptr<net::StreamSocket> connecting_socket_;
  base::OneShotTimer connection_timeout_;

  bool log_connection_failure_ = true;
  bool log_timeout_ = true;

  base::WeakPtrFactory<OutputConnection> weak_factory_{this};
};

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_OUTPUT_CONNECTION_H_
