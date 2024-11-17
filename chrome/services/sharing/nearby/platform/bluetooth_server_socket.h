// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_SERVER_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_SERVER_SOCKET_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/bluetooth_classic.h"

namespace nearby::chrome {

// Concrete BluetoothServerSocket implementation.
class BluetoothServerSocket : public api::BluetoothServerSocket {
 public:
  explicit BluetoothServerSocket(
      mojo::PendingRemote<bluetooth::mojom::ServerSocket> server_socket);
  ~BluetoothServerSocket() override;

  BluetoothServerSocket(const BluetoothServerSocket&) = delete;
  BluetoothServerSocket& operator=(const BluetoothServerSocket&) = delete;

  // api::BluetoothServerSocket:
  std::unique_ptr<api::BluetoothSocket> Accept() override;
  Exception Close() override;

 private:
  // BluetoothServerSocket is created on the main thread, but its public methods
  // are used on a separate dedicated thread. A mojo::ShareRemote object
  // |server_socket_| is used to ensure that all calls on the mojo happen on
  // the dedicated task runner/sequence regardless of the calling thread. This
  // is necessary because bluetooth::mojom::ServerSocket.Accept() is a [Sync]
  // mojo method and we have previously observed deadlock in the context of
  // Nearby Connections without a SharedRemote implementation.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::SharedRemote<bluetooth::mojom::ServerSocket> server_socket_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_SERVER_SOCKET_H_
