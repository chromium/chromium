// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_SOCKET_H_

#include <string>

#include "base/optional.h"
#include "chrome/services/sharing/nearby/platform_v2/bluetooth_device.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/cpp/platform_v2/api/bluetooth_classic.h"
#include "third_party/nearby/src/cpp/platform_v2/base/input_stream.h"
#include "third_party/nearby/src/cpp/platform_v2/base/output_stream.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace location {
namespace nearby {
namespace chrome {

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
//
// api::BluetoothSocket's subclasses are also synchronous interfaces, but the
// Mojo DataPipes they consume only provide asynchronous interfaces. This is
// reconciled by blocking on the caller thread when waiting (via a
// mojo::SimpleWatcher) for the DataPipes to become readable or writable (this
// is expected by the callers of api::BluetoothSocket's subclasses). Mojo
// DataPipe operations are handled on a separate task runner (see
// |task_runner_|) so that blocking on the calling thread will not deadlock.
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
  void InitializeStreams(mojo::ScopedDataPipeConsumerHandle receive_stream,
                         mojo::ScopedDataPipeProducerHandle send_stream);

  // These methods must be run on |task_runner_|, because the
  // mojo::SimpleWatcher members of |input_stream_| and |output_stream_| expect
  // to be created on the same sequence they are later run on. See
  // |task_runner_|.
  void CreateInputStream(mojo::ScopedDataPipeConsumerHandle receive_stream,
                         base::OnceClosure callback);
  void DestroyInputStream(base::OnceClosure callback);
  void CreateOutputStream(mojo::ScopedDataPipeProducerHandle send_stream,
                          base::OnceClosure callback);
  void DestroyOutputStream(base::OnceClosure callback);

  // If this BluetoothSocket is created by connecting to a discovered device, a
  // reference to that device will be provided to set |remote_device_ref_|, and
  // |remote_device_| will be left unset.
  // On the other hand, if this BluetoothSocket is created by an incoming
  // connection, there is no previous owner of that device object, and therefore
  // BluetoothSocket is expected to own it (within |remote_device_|). In this
  // case, |remote_device_ref_| is a reference to |remote_device_|.
  base::Optional<chrome::BluetoothDevice> remote_device_;
  api::BluetoothDevice& remote_device_ref_;

  // The public methods which are overridden by BluetoothSocket's subclasses
  // InputStreamImpl and OutputStreamImpl are expected to block the caller
  // thread. While that thread is blocked, |task_runner_| handles read and write
  // operations on |input_stream_| and |output_stream_|. Because of that,
  // |input_stream_| (and its mojo::SimpleWatcher) and |output_stream_| (and
  // its mojo::SimpleWatcher), must be created and destroyed on |task_runner_|
  // (see respective helper Create* and Destroy* methods).
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // These properties must be created on |task_runner_|. See |task_runner_|.
  mojo::SharedRemote<bluetooth::mojom::Socket> socket_;
  std::unique_ptr<InputStream> input_stream_;
  std::unique_ptr<OutputStream> output_stream_;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_SOCKET_H_
