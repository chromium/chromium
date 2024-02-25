// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_INITIALIZER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_INITIALIZER_H_

#include <memory>
#include <queue>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/secure_channel_base.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {
class BluetoothAdapter;
}

namespace ash::secure_channel {

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
    static std::unique_ptr<SecureChannelBase> Create(
        scoped_refptr<base::TaskRunner> task_runner =
            base::SingleThreadTaskRunner::GetCurrentDefault());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<SecureChannelBase> CreateInstance(
        scoped_refptr<base::TaskRunner> task_runner) = 0;

   private:
    static Factory* test_factory_;
  };

  SecureChannelInitializer(const SecureChannelInitializer&) = delete;
  SecureChannelInitializer& operator=(const SecureChannelInitializer&) = delete;

  ~SecureChannelInitializer() override;

 private:
  explicit SecureChannelInitializer(
      scoped_refptr<base::TaskRunner> task_runner);

  struct ConnectionRequestArgs {
    ConnectionRequestArgs(
        const multidevice::RemoteDevice& device_to_connect,
        const multidevice::RemoteDevice& local_device,
        const std::string& feature,
        ConnectionMedium connection_medium,
        ConnectionPriority connection_priority,
        mojo::PendingRemote<mojom::ConnectionDelegate> delegate,
        mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
            secure_channel_structured_metrics_logger,
        bool is_listen_request);
    ~ConnectionRequestArgs();

    multidevice::RemoteDevice device_to_connect;
    multidevice::RemoteDevice local_device;
    std::string feature;
    ConnectionMedium connection_medium;
    ConnectionPriority connection_priority;
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate;
    mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
        secure_channel_structured_metrics_logger;
    bool is_listen_request;
  };

  // mojom::SecureChannel:
  void ListenForConnectionFromDevice(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      mojo::PendingRemote<mojom::ConnectionDelegate> delegate) override;
  void InitiateConnectionToDevice(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      mojo::PendingRemote<mojom::ConnectionDelegate> delegate,
      mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
          secure_channel_structured_metrics_logger) override;
  void SetNearbyConnector(
      mojo::PendingRemote<mojom::NearbyConnector> nearby_connector) override;
  void GetLastSeenTimestamp(
      const std::string& remote_device_id,
      base::OnceCallback<void(std::optional<base::Time>)> callback) override;

  void OnBluetoothAdapterReceived(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  mojo::PendingRemote<mojom::NearbyConnector> nearby_connector_;
  std::queue<std::unique_ptr<ConnectionRequestArgs>> pending_args_;
  std::unique_ptr<mojom::SecureChannel> secure_channel_impl_;

  base::WeakPtrFactory<SecureChannelInitializer> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_INITIALIZER_H_
