// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/audio_socket_broker.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/unix_domain_socket.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chromecast/media/audio/audio_io_thread.h"
#include "chromecast/media/audio/audio_output_service/constants.h"
#include "chromecast/net/socket_util.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "net/base/net_errors.h"
#include "net/socket/unix_domain_client_socket_posix.h"

namespace chromecast {
namespace media {

namespace {

constexpr base::TimeDelta kConnectTimeout = base::Seconds(1);
constexpr char kSocketMsg[] = "socket-handle";

}  // namespace

// Helper class for sending the socket descriptors to the audio output service.
class AudioSocketBroker::SocketFdConnection {
 public:
  SocketFdConnection(AudioSocketBroker* socket_broker,
                     base::ScopedFD connect_socket_fd,
                     base::ScopedFD pending_socket_fd,
                     const std::string& audio_output_service_path,
                     base::OnceCallback<void(base::ScopedFD)> connect_callback)
      : socket_broker_(socket_broker),
        socket_fd_(std::move(connect_socket_fd)),
        pending_socket_fd_(std::move(pending_socket_fd)),
        connect_callback_(std::move(connect_callback)) {
    DCHECK(socket_broker_);
    DCHECK(socket_fd_.is_valid());
    DCHECK(pending_socket_fd_.is_valid());
    DCHECK(connect_callback_);
    connecting_socket_ = std::make_unique<net::UnixDomainClientSocket>(
        audio_output_service_path, true);
  }
  SocketFdConnection(const SocketFdConnection&) = delete;
  SocketFdConnection& operator=(const SocketFdConnection&) = delete;
  ~SocketFdConnection() = default;

  void Connect() {
    DCHECK(connecting_socket_);
    int result = connecting_socket_->Connect(base::BindOnce(
        &SocketFdConnection::OnConnected, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING) {
      OnConnected(result);
      return;
    }

    connection_timeout_.Start(FROM_HERE, kConnectTimeout, this,
                              &SocketFdConnection::ConnectTimeout);
  }

 private:
  void OnConnected(int result) {
    if (result != net::OK ||
        !base::UnixDomainSocket::SendMsg(
            connecting_socket_->ReleaseConnectedSocket(), kSocketMsg,
            sizeof(kSocketMsg), {socket_fd_.get()})) {
      std::move(connect_callback_).Run(base::ScopedFD());
      return;
    }
    connecting_socket_.reset();
    std::move(connect_callback_).Run(std::move(pending_socket_fd_));
  }

  void ConnectTimeout() {
    LOG(ERROR) << "Timed out connecting to audio output service";
    OnConnected(net::ERR_TIMED_OUT);
  }

  AudioSocketBroker* const socket_broker_;
  base::ScopedFD socket_fd_;
  base::ScopedFD pending_socket_fd_;
  base::OnceCallback<void(base::ScopedFD)> connect_callback_;
  std::unique_ptr<net::UnixDomainClientSocket> connecting_socket_;
  base::OneShotTimer connection_timeout_;
};

AudioSocketBroker::PendingConnectionInfo::PendingConnectionInfo(
    base::SequenceBound<SocketFdConnection> arg_socket_fd_connection,
    GetSocketDescriptorCallback arg_callback)
    : socket_fd_connection(std::move(arg_socket_fd_connection)),
      callback(std::move(arg_callback)) {}

AudioSocketBroker::PendingConnectionInfo::PendingConnectionInfo(
    PendingConnectionInfo&&) = default;
AudioSocketBroker::PendingConnectionInfo&
AudioSocketBroker::PendingConnectionInfo::operator=(PendingConnectionInfo&&) =
    default;

AudioSocketBroker::PendingConnectionInfo::~PendingConnectionInfo() = default;

void AudioSocketBroker::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::AudioSocketBroker> receiver) {
  CHECK(render_frame_host);
  // Lifecycle managed by content::DocumentService.
  new AudioSocketBroker(*render_frame_host, std::move(receiver));
}

AudioSocketBroker& AudioSocketBroker::CreateForTesting(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::AudioSocketBroker> receiver,
    const std::string& audio_output_service_path) {
  return *new AudioSocketBroker(render_frame_host, std::move(receiver),
                                audio_output_service_path);
}

AudioSocketBroker::AudioSocketBroker(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::AudioSocketBroker> receiver)
    : AudioSocketBroker(render_frame_host,
                        std::move(receiver),
                        audio_output_service::
                            kDefaultAudioOutputServiceUnixDomainSocketPath) {}

AudioSocketBroker::AudioSocketBroker(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::AudioSocketBroker> receiver,
    const std::string& audio_output_service_path)
    : DocumentService(render_frame_host, std::move(receiver)),
      audio_output_service_path_(audio_output_service_path),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

AudioSocketBroker::~AudioSocketBroker() = default;

void AudioSocketBroker::GetSocketDescriptor(
    GetSocketDescriptorCallback callback) {
  base::ScopedFD socket_fd1, socket_fd2;
  if (!CreateUnnamedSocketPair(&socket_fd1, &socket_fd2)) {
    std::move(callback).Run(mojo::PlatformHandle(base::ScopedFD()));
    return;
  }

  // Send one socket descriptor to audio output service first, and then the
  // other to the client in the renderer.
  int sock_fd1 = socket_fd1.get();
  auto socket_fd_connection = base::SequenceBound<SocketFdConnection>(
      AudioIoThread::Get()->task_runner(), this, std::move(socket_fd2),
      std::move(socket_fd1), audio_output_service_path_,
      base::BindPostTask(
          main_task_runner_,
          base::BindOnce(&AudioSocketBroker::OnSocketHandleSentToAudioService,
                         weak_factory_.GetWeakPtr(), sock_fd1)));
  auto result = pending_connection_infos_.emplace(
      sock_fd1, PendingConnectionInfo(std::move(socket_fd_connection),
                                      std::move(callback)));
  DCHECK(result.second);
  result.first->second.socket_fd_connection.AsyncCall(
      &SocketFdConnection::Connect);
}

void AudioSocketBroker::OnSocketHandleSentToAudioService(
    int socket_fd,
    base::ScopedFD pending_socket_fd) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  auto it = pending_connection_infos_.find(socket_fd);
  if (it == pending_connection_infos_.end()) {
    LOG(ERROR) << "Cannot find connection: " << socket_fd;
    return;
  }

  DCHECK(it->second.callback);

  // Now that one descriptor is sent to the audio output service, send the other
  // descriptor back to the client.
  std::move(it->second.callback)
      .Run(mojo::PlatformHandle(std::move(pending_socket_fd)));
  pending_connection_infos_.erase(it);
}

}  // namespace media
}  // namespace chromecast
