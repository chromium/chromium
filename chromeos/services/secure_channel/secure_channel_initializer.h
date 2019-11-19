// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_INITIALIZER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_INITIALIZER_H_

#include <memory>
#include <queue>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/secure_channel_base.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace chromeos {

namespace secure_channel {

// SecureChannelBase implementation which fetches the Bluetooth adapter, then
// initializes the rest of the service. Initialization of the service is
// asynchronous due to the need to fetch the Bluetooth adapter asynchronously.
// This class allows clients to make requests of the service before it is fully
// initializes; queued requests are then passed on to the rest of the service
// once initialization completes.
class SecureChannelInitializer : public SecureChannelBase {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<SecureChannelBase> BuildInstance(
        scoped_refptr<base::TaskRunner> task_runner =
            base::ThreadTaskRunnerHandle::Get());

   private:
    static Factory* test_factory_;
  };

  ~SecureChannelInitializer() override;

 private:
  explicit SecureChannelInitializer(
      scoped_refptr<base::TaskRunner> task_runner);

  struct ConnectionRequestArgs {
    ConnectionRequestArgs(
        const multidevice::RemoteDevice& device_to_connect,
        const multidevice::RemoteDevice& local_device,
        const std::string& feature,
        ConnectionPriority connection_priority,
        mojo::PendingRemote<mojom::ConnectionDelegate> delegate,
        bool is_listen_request);
    ~ConnectionRequestArgs();

    multidevice::RemoteDevice device_to_connect;
    multidevice::RemoteDevice local_device;
    std::string feature;
    ConnectionPriority connection_priority;
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate;
    bool is_listen_request;
  };

  // mojom::SecureChannel:
  void ListenForConnectionFromDevice(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      mojo::PendingRemote<mojom::ConnectionDelegate> delegate) override;
  void InitiateConnectionToDevice(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      mojo::PendingRemote<mojom::ConnectionDelegate> delegate) override;

  void OnBluetoothAdapterReceived(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  std::queue<std::unique_ptr<ConnectionRequestArgs>> pending_args_;
  std::unique_ptr<mojom::SecureChannel> secure_channel_impl_;

  base::WeakPtrFactory<SecureChannelInitializer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SecureChannelInitializer);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_INITIALIZER_H_
