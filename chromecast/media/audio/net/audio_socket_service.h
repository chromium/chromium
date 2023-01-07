// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_NET_AUDIO_SOCKET_SERVICE_H_
#define CHROMECAST_MEDIA_AUDIO_NET_AUDIO_SOCKET_SERVICE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/socket/socket_descriptor.h"

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

  // When |use_socket_descriptor| is true, AudioSocketService will receive a
  // socket descriptor from the connection created through |listen_socket_|, and
  // then use the received socket descriptor to create the real socket for
  // transferring data. This is useful when the actual client of the service is
  // not able to create a socket themselves but instead needs a brokered socket
  // descriptor (created with socketpair()) to connect.
  AudioSocketService(const std::string& endpoint,
                     int port,
                     int max_accept_loop,
                     Delegate* delegate,
                     bool use_socket_descriptor = false);
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
  void OnAsyncAcceptComplete(int result);
  bool HandleAcceptResult(int result);

  // The following methods are implemented in audio_socket_service_{uds|tcp}.cc.
  int AcceptOne();
  void OnAcceptSuccess();
  void ReceiveFdFromSocket(int socket_fd);

  const int max_accept_loop_;
  const bool use_socket_descriptor_;
  Delegate* const delegate_;  // Not owned.

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<net::ServerSocket> listen_socket_;
  std::unique_ptr<net::StreamSocket> accepted_socket_;
  net::SocketDescriptor accepted_descriptor_ = net::kInvalidSocket;
  base::flat_map<int /* fd */,
                 std::unique_ptr<base::FileDescriptorWatcher::Controller>>
      fd_watcher_controllers_;
  base::WeakPtrFactory<AudioSocketService> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_NET_AUDIO_SOCKET_SERVICE_H_
