// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class TaskRunner;
}

namespace ash {

namespace multidevice {
class RemoteDeviceRef;
}

namespace secure_channel {

// Provides clients access to the SecureChannel API.
class SecureChannelClientImpl : public SecureChannelClient {
 public:
  class Factory {
   public:
    static std::unique_ptr<SecureChannelClient> Create(
        mojo::PendingRemote<mojom::SecureChannel> channel,
        scoped_refptr<base::TaskRunner> task_runner =
            base::SingleThreadTaskRunner::GetCurrentDefault());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<SecureChannelClient> CreateInstance(
        mojo::PendingRemote<mojom::SecureChannel> channel,
        scoped_refptr<base::TaskRunner> task_runner) = 0;

   private:
    static Factory* test_factory_;
  };

  SecureChannelClientImpl(const SecureChannelClientImpl&) = delete;
  SecureChannelClientImpl& operator=(const SecureChannelClientImpl&) = delete;

  ~SecureChannelClientImpl() override;

 private:
  friend class SecureChannelClientImplTest;

  SecureChannelClientImpl(mojo::PendingRemote<mojom::SecureChannel> channel,
                          scoped_refptr<base::TaskRunner> task_runner);

  // SecureChannelClient:
  std::unique_ptr<ConnectionAttempt> InitiateConnectionToDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      SecureChannelStructuredMetricsLogger*
          secure_channel_structure_metrics_logger) override;
  std::unique_ptr<ConnectionAttempt> ListenForConnectionFromDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) override;
  void SetNearbyConnector(NearbyConnector* nearby_connector) override;
  void GetLastSeenTimestamp(
      const std::string& remote_device_id,
      base::OnceCallback<void(std::optional<base::Time>)> callback) override;

  void PerformInitiateConnectionToDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote,
      mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
          secure_channel_structured_metrics_logger_remote);
  void PerformListenForConnectionFromDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      mojo::PendingRemote<mojom::ConnectionDelegate>
          connection_delegate_remote);

  void FlushForTesting();

  mojo::Remote<mojom::SecureChannel> secure_channel_remote_;

  scoped_refptr<base::TaskRunner> task_runner_;

  base::WeakPtrFactory<SecureChannelClientImpl> weak_ptr_factory_{this};
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_IMPL_H_
