// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_output_service/receiver/receiver.h"

#include <utility>

#include "chromecast/media/audio/audio_output_service/audio_output_service.pb.h"
#include "chromecast/media/audio/audio_output_service/output_socket.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace audio_output_service {

namespace {

constexpr int kMaxAcceptLoop = 5;

}  // namespace

class Receiver::InitialSocket : public OutputSocket::Delegate {
 public:
  InitialSocket(Receiver* receiver, std::unique_ptr<OutputSocket> socket)
      : receiver_(receiver), socket_(std::move(socket)) {
    DCHECK(receiver_);
    socket_->SetDelegate(this);
  }

  InitialSocket(const InitialSocket&) = delete;
  InitialSocket& operator=(const InitialSocket&) = delete;
  ~InitialSocket() override = default;

 private:
  // OutputSocket::Delegate implementation:
  bool HandleMetadata(const Generic& message) override {
    if (message.has_backend_params()) {
      receiver_->CreateOutputStream(std::move(socket_), message);
      receiver_->RemoveInitialSocket(this);
    }

    return true;
  }

  void OnConnectionError() override { receiver_->RemoveInitialSocket(this); }

  Receiver* const receiver_;
  std::unique_ptr<OutputSocket> socket_;
};

Receiver::Receiver(const std::string& uds_path, int tcp_port)
    : socket_service_(uds_path,
                      tcp_port,
                      kMaxAcceptLoop,
                      this,
                      /*use_socket_descriptor=*/true) {
  socket_service_.Accept();
}

Receiver::~Receiver() = default;

void Receiver::HandleAcceptedSocket(std::unique_ptr<net::StreamSocket> socket) {
  AddInitialSocket(std::make_unique<InitialSocket>(
      this, std::make_unique<OutputSocket>(std::move(socket))));
}

void Receiver::AddInitialSocket(std::unique_ptr<InitialSocket> initial_socket) {
  InitialSocket* ptr = initial_socket.get();
  initial_sockets_[ptr] = std::move(initial_socket);
}

void Receiver::RemoveInitialSocket(InitialSocket* initial_socket) {
  initial_sockets_.erase(initial_socket);
}

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast
