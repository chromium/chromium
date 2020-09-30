// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/bluetooth_server_socket.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/services/sharing/nearby/platform_v2/bluetooth_socket.h"
#include "third_party/nearby/src/cpp/platform_v2/base/exception.h"

namespace location {
namespace nearby {
namespace chrome {

BluetoothServerSocket::BluetoothServerSocket(
    mojo::PendingRemote<bluetooth::mojom::ServerSocket> server_socket)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      server_socket_(std::move(server_socket), task_runner_) {}

BluetoothServerSocket::~BluetoothServerSocket() = default;

std::unique_ptr<api::BluetoothSocket> BluetoothServerSocket::Accept() {
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
  server_socket_.reset();
  return {Exception::kSuccess};
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
