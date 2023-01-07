// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_AUDIO_SOCKET_BROKER_H_
#define CHROMECAST_BROWSER_AUDIO_SOCKET_BROKER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "chromecast/common/mojom/audio_socket.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace base {
class SequencedTaskRunner;
}  //  namespace base

namespace net {
class UnixDomainClientSocket;
}  // namespace net

namespace chromecast {
namespace media {

// Service hosted in the browser process to provide the descriptors of connected
// Unix Domain sockets for renderers and the audio output service. This service
// is necessary since renderers are not allowed to perform socket operations on
// some platforms (e.g. Android).
class AudioSocketBroker
    : public ::content::DocumentService<mojom::AudioSocketBroker> {
 public:
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::AudioSocketBroker> receiver);
  static AudioSocketBroker& CreateForTesting(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::AudioSocketBroker> receiver,
      const std::string& audio_output_service_path);
  AudioSocketBroker(const AudioSocketBroker&) = delete;
  AudioSocketBroker& operator=(const AudioSocketBroker&) = delete;

 private:
  class SocketFdConnection;

  AudioSocketBroker(content::RenderFrameHost& render_frame_host,
                    mojo::PendingReceiver<mojom::AudioSocketBroker> receiver);
  AudioSocketBroker(content::RenderFrameHost& render_frame_host,
                    mojo::PendingReceiver<mojom::AudioSocketBroker> receiver,
                    const std::string& audio_output_service_path);
  ~AudioSocketBroker() override;

  // Helper struct which holds the information regarding a socket pair
  // in the time between sending the socket to the renderer and the audio
  // service.
  struct PendingConnectionInfo {
    PendingConnectionInfo(
        base::SequenceBound<SocketFdConnection> arg_socket_fd_connection,
        GetSocketDescriptorCallback arg_callback);
    PendingConnectionInfo(const PendingConnectionInfo&) = delete;
    PendingConnectionInfo& operator=(const PendingConnectionInfo&) = delete;
    PendingConnectionInfo(PendingConnectionInfo&&);
    PendingConnectionInfo& operator=(PendingConnectionInfo&&);
    ~PendingConnectionInfo();

    base::SequenceBound<SocketFdConnection> socket_fd_connection;
    GetSocketDescriptorCallback callback;
  };

  // mojom::AudioSocketBroker implementation:
  void GetSocketDescriptor(GetSocketDescriptorCallback callback) override;

  // Callback triggered when the socket handle is sent to the audio output
  // service. |socket_fd| is the key to |pending_connection_infos_|.
  // |pending_socket_fd| is invalid when connection fails.
  void OnSocketHandleSentToAudioService(int socket_fd,
                                        base::ScopedFD pending_socket_fd);

  const std::string audio_output_service_path_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  base::flat_map<int /* socket FD */, PendingConnectionInfo>
      pending_connection_infos_;

  base::WeakPtrFactory<AudioSocketBroker> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_AUDIO_SOCKET_BROKER_H_
