// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_server_socket.h"

#include "base/threading/thread_restrictions.h"

namespace nearby::chrome {

WifiDirectServerSocket::WifiDirectServerSocket(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PlatformHandle handle,
    std::unique_ptr<net::TCPServerSocket> tcp_server_socket)
    : task_runner_(std::move(task_runner)),
      handle_(std::move(handle)),
      tcp_server_socket_(std::move(tcp_server_socket)) {}

WifiDirectServerSocket::~WifiDirectServerSocket() {
  Close();
}

// api::WifiDirectServerSocket
std::string WifiDirectServerSocket::GetIPAddress() const {
  NOTIMPLEMENTED();
  return std::string();
}

int WifiDirectServerSocket::GetPort() const {
  NOTIMPLEMENTED();
  return -1;
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectServerSocket::Accept() {
  NOTIMPLEMENTED();
  return nullptr;
}

Exception WifiDirectServerSocket::Close() {
  handle_.reset();

  if (!tcp_server_socket_) {
    return {Exception::kFailed};
  }

  // Cleanup the socket on the IO thread.
  base::WaitableEvent waitable_event;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WifiDirectServerSocket::CloseSocket,
                                base::Unretained(this), &waitable_event));
  waitable_event.Wait();

  return {Exception::kSuccess};
}

void WifiDirectServerSocket::CloseSocket(
    base::WaitableEvent* close_waitable_event) {
  tcp_server_socket_.reset();
  close_waitable_event->Signal();
}

}  // namespace nearby::chrome
