// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_socket.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/services/sharing/nearby/platform/bidirectional_stream.h"

namespace nearby::chrome {

BluetoothSocket::BluetoothSocket(
    bluetooth::mojom::DeviceInfoPtr device,
    mojo::PendingRemote<bluetooth::mojom::Socket> socket,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream)
    : remote_device_(std::move(device)),
      remote_device_ref_(*remote_device_),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      socket_(std::move(socket), task_runner_),
      bidirectional_stream_(connections::mojom::Medium::kBluetooth,
                            task_runner_,
                            std::move(receive_stream),
                            std::move(send_stream)) {}

BluetoothSocket::BluetoothSocket(
    api::BluetoothDevice& remote_device,
    mojo::PendingRemote<bluetooth::mojom::Socket> socket,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream)
    : remote_device_ref_(remote_device),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      socket_(std::move(socket), task_runner_),
      bidirectional_stream_(connections::mojom::Medium::kBluetooth,
                            task_runner_,
                            std::move(receive_stream),
                            std::move(send_stream)) {}

BluetoothSocket::~BluetoothSocket() {
  Close();
}

InputStream& BluetoothSocket::GetInputStream() {
  DCHECK(bidirectional_stream_.GetInputStream());
  return *bidirectional_stream_.GetInputStream();
}

OutputStream& BluetoothSocket::GetOutputStream() {
  DCHECK(bidirectional_stream_.GetOutputStream());
  return *bidirectional_stream_.GetOutputStream();
}

Exception BluetoothSocket::Close() {
  CloseMojoSocketIfNecessary();

  return bidirectional_stream_.Close();
}

api::BluetoothDevice* BluetoothSocket::GetRemoteDevice() {
  return &*remote_device_ref_;
}

void BluetoothSocket::CloseMojoSocketIfNecessary() {
  base::AutoLock lock(lock_);

  if (!socket_)
    return;

  // TODO(crbug.com/40057928): Remove CHECKs when crash fix is verified.
  // If not for the lock--or if thread safety is violated in some unexpected
  // way--these CHECKs would be triggered when Close() is called simultaneously
  // from multiple threads.
  CHECK(socket_);
  socket_->Disconnect();
  CHECK(socket_);
  socket_.reset();
}

}  // namespace nearby::chrome
