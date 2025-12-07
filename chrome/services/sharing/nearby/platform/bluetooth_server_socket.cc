// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_server_socket.h"

#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_socket.h"
#include "third_party/nearby/src/internal/platform/exception.h"

namespace nearby::chrome {

BluetoothServerSocket::BluetoothServerSocket(
    mojo::PendingRemote<bluetooth::mojom::ServerSocket> server_socket)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      server_socket_(std::move(server_socket), task_runner_) {}

BluetoothServerSocket::~BluetoothServerSocket() {
  Close();
}

std::unique_ptr<api::BluetoothSocket> BluetoothServerSocket::Accept() {
  // Check if Close() has already been called which can happen when quickly
  // toggling between high-viz and contact based advertising.
  if (!server_socket_) {
    VLOG(1) << "BluetoothServerSocket::Accept() called but mojo remote was"
            << " already closed.";
    return nullptr;
  }

  bluetooth::mojom::AcceptConnectionResultPtr result;
  bool success = server_socket_->Accept(&result);

  if (success && result) {
    return std::make_unique<chrome::BluetoothSocket>(
        std::move(result->device), std::move(result->socket),
        std::move(result->receive_stream), std::move(result->send_stream));
  }

  return nullptr;
}

Exception BluetoothServerSocket::Close() {
  if (server_socket_) {
    // Release |server_socket_| and only keep it for the remainder of this
    // method. This prevents (expected) re-entry into this method from
    // triggering multiple calls to ServerSocket::Disconnect().
    auto server_socket_copy = std::move(server_socket_);

    if (server_socket_copy->Disconnect()) {
      VLOG(1) << "Successfully tore down Nearby Bluetooth server socket.";
    } else {
      LOG(ERROR) << "Failed to tear down Nearby Bluetooth server socket.";
    }
  }
  return {Exception::kSuccess};
}

}  // namespace nearby::chrome
