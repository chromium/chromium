// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_SOCKET_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "chrome/services/sharing/nearby/platform/bidirectional_stream.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_device.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/bluetooth_classic.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace nearby::chrome {

// Concrete BluetoothSocket implementation.
//
// Nearby Connections uses this BluetoothSocket to communicate with a remote
// device. At a high-level, it first exchanges frames to encrypt the connection,
// then delegates to its caller (e.g., Nearby Share) to authenticate the
// connection, and finally expects its caller to send and receive
// application-level messages. The following precautions are taken to handle
// untrusted bytes received from remote devices:
//   * Nearby Connections (which this class is a piece of) is hosted in a
//     Nearby utility process.
//   * Whenever Nearby Connections provides bytes received from a remote device
//     to its caller, even after it has been authenticated, the caller must not
//     trust those bytes, and is responsible for first passing those bytes
//     through the trusted NearbyDecoder interface (see go/nearby-chrome-mojo).
//     NearbyDecoder is hosted in the same Nearby utility process.
//
// api::BluetoothSocket is a synchronous interface, so this implementation
// consumes the synchronous signatures of bluetooth::mojom::Socket methods.
class BluetoothSocket : public api::BluetoothSocket {
 public:
  BluetoothSocket(bluetooth::mojom::DeviceInfoPtr device,
                  mojo::PendingRemote<bluetooth::mojom::Socket> socket,
                  mojo::ScopedDataPipeConsumerHandle receive_stream,
                  mojo::ScopedDataPipeProducerHandle send_stream);
  BluetoothSocket(api::BluetoothDevice& remote_device,
                  mojo::PendingRemote<bluetooth::mojom::Socket> socket,
                  mojo::ScopedDataPipeConsumerHandle receive_stream,
                  mojo::ScopedDataPipeProducerHandle send_stream);
  ~BluetoothSocket() override;

  BluetoothSocket(const BluetoothSocket&) = delete;
  BluetoothSocket& operator=(const BluetoothSocket&) = delete;

  // api::BluetoothSocket:
  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override;
  api::BluetoothDevice* GetRemoteDevice() override;

 private:
  void CloseMojoSocketIfNecessary();

  // Protects |socket_| while closing.
  base::Lock lock_;

  // If this BluetoothSocket is created by connecting to a discovered device, a
  // reference to that device will be provided to set |remote_device_ref_|, and
  // |remote_device_| will be left unset.
  // On the other hand, if this BluetoothSocket is created by an incoming
  // connection, there is no previous owner of that device object, and therefore
  // BluetoothSocket is expected to own it (within |remote_device_|). In this
  // case, |remote_device_ref_| is a reference to |remote_device_|.
  std::optional<chrome::BluetoothDevice> remote_device_;
  const raw_ref<api::BluetoothDevice> remote_device_ref_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::SharedRemote<bluetooth::mojom::Socket> socket_;
  BidirectionalStream bidirectional_stream_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_SOCKET_H_
