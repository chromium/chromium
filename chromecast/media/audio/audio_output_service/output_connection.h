// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_OUTPUT_CONNECTION_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_OUTPUT_CONNECTION_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromecast/common/mojom/audio_socket.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace chromecast {
namespace media {
namespace audio_output_service {
class OutputSocket;

// Base class for connecting to the audio output service.
// Since this class lives in renderers, to avoid creating sockets directly, it
// uses AudioSocketBroker to get socket handles.
class OutputConnection {
 public:
  explicit OutputConnection(
      mojo::PendingRemote<mojom::AudioSocketBroker> pending_socket_broker);
  OutputConnection(const OutputConnection&) = delete;
  OutputConnection& operator=(const OutputConnection&) = delete;
  virtual ~OutputConnection();

  // Initiates connection to the audio output service. Will call OnConnected()
  // when connection is successful.
  void Connect();

 protected:
  // Called when a connection is established to the audio output service.
  virtual void OnConnected(std::unique_ptr<OutputSocket> socket) = 0;

  // Called when the connection failed (after retries).
  virtual void OnConnectionFailed() = 0;

 private:
  void HandleConnectResult(int result);
  void OnSocketDescriptor(mojo::PlatformHandle handle);

  const mojo::Remote<mojom::AudioSocketBroker> socket_broker_;
  std::unique_ptr<net::StreamSocket> connecting_socket_;

  bool log_connection_failure_ = true;
  int retry_count_ = 0;

  base::WeakPtrFactory<OutputConnection> weak_factory_{this};
};

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_OUTPUT_CONNECTION_H_
