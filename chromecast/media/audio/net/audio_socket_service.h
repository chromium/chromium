// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_NET_AUDIO_SOCKET_SERVICE_H_
#define CHROMECAST_MEDIA_AUDIO_NET_AUDIO_SOCKET_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace net {
class ServerSocket;
class StreamSocket;
}  // namespace net

namespace chromecast {
namespace media {

// Listens to a server socket and passes accepted sockets to a delegate. It
// is used for creating socket connections to pass audio data between processes.
// Must be created and used on an IO thread.
class AudioSocketService {
 public:
  class Delegate {
   public:
    // Handles a newly accepted |socket|.
    virtual void HandleAcceptedSocket(
        std::unique_ptr<net::StreamSocket> socket) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  AudioSocketService(const std::string& endpoint,
                     int port,
                     int max_accept_loop,
                     Delegate* delegate);
  AudioSocketService(const AudioSocketService&) = delete;
  AudioSocketService& operator=(const AudioSocketService&) = delete;
  ~AudioSocketService();

  // Starts accepting incoming connections.
  void Accept();

  // Creates a connection to an AudioSocketService instance. The |endpoint| is
  // used on systems that support Unix domain sockets; otherwise, the |port| is
  // used to make a TCP connection.
  static std::unique_ptr<net::StreamSocket> Connect(const std::string& endpoint,
                                                    int port);

 private:
  void OnAccept(int result);
  bool HandleAcceptResult(int result);

  const int max_accept_loop_;
  Delegate* const delegate_;  // Not owned.

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<net::ServerSocket> listen_socket_;
  std::unique_ptr<net::StreamSocket> accepted_socket_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_NET_AUDIO_SOCKET_SERVICE_H_
