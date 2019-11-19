// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_IMPL_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chromeos/components/multidevice/remote_device_cache.h"
#include "chromeos/services/secure_channel/active_connection_manager.h"
#include "chromeos/services/secure_channel/connection_attempt_details.h"
#include "chromeos/services/secure_channel/pending_connection_manager.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace chromeos {

namespace secure_channel {

class BleConnectionManager;
class BleServiceDataHelper;
class TimerFactory;

// Concrete SecureChannelImpl implementation, which contains three pieces:
// (1) PendingConnectionManager: Attempts to create connections to remote
//     devices.
// (2) ActiveConnectionManager: Maintains connections to remote devices, sharing
//     a single connection with multiple clients when appropriate.
// (3) RemoteDeviceCache: Caches devices within this service.
class SecureChannelImpl : public mojom::SecureChannel,
                          public ActiveConnectionManager::Delegate,
                          public PendingConnectionManager::Delegate {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<mojom::SecureChannel> BuildInstance(
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

   private:
    static Factory* test_factory_;
  };

  ~SecureChannelImpl() override;

 private:
  explicit SecureChannelImpl(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  enum class InvalidRemoteDeviceReason { kInvalidPublicKey, kInvalidPsk };

  enum class ApiFunctionName { kListenForConnection, kInitiateConnection };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const ApiFunctionName& role);

  // Contains metadata related to connection requests that were attempted while
  // an ongoing connection to the same remote device was in the process of
  // disconnecting. When this situation occurs, we must wait for the existing
  // connection to disconnect fully, then initiate a new connection attempt to
  // that device.
  struct ConnectionRequestWaitingForDisconnection {
    ConnectionRequestWaitingForDisconnection(
        std::unique_ptr<ClientConnectionParameters>
            client_connection_parameters,
        ConnectionAttemptDetails connection_attempt_details,
        ConnectionPriority connection_priority);
    ConnectionRequestWaitingForDisconnection(
        ConnectionRequestWaitingForDisconnection&& other) noexcept;
    ConnectionRequestWaitingForDisconnection& operator=(
        ConnectionRequestWaitingForDisconnection&& other) noexcept;
    ~ConnectionRequestWaitingForDisconnection();

    std::unique_ptr<ClientConnectionParameters> client_connection_parameters;
    ConnectionAttemptDetails connection_attempt_details;
    ConnectionPriority connection_priority;
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

  // ActiveConnectionManager::Delegate:
  void OnDisconnected(const ConnectionDetails& connection_details) override;

  // PendingConnectionManager::Delegate:
  void OnConnection(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel,
      std::vector<std::unique_ptr<ClientConnectionParameters>> clients,
      const ConnectionDetails& connection_details) override;

  void ProcessConnectionRequest(
      ApiFunctionName api_fn_name,
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionRole connection_role,
      ConnectionPriority connection_priority,
      ConnectionMedium connection_medium);
  void RejectRequestForReason(
      ApiFunctionName api_fn_name,
      mojom::ConnectionAttemptFailureReason reason,
      ClientConnectionParameters* client_connection_parameters);

  // Checks if |client_connection_parameters| is invalid. Returns whether
  // Returns whether the request was rejected.
  bool CheckForInvalidRequest(
      ApiFunctionName api_fn_name,
      ClientConnectionParameters* client_connection_parameters) const;

  // Checks if |device| is invalid, and rejects the connection request if so.
  // Returns whether the request was rejected.
  bool CheckForInvalidInputDevice(
      ApiFunctionName api_fn_name,
      const multidevice::RemoteDevice& device,
      ClientConnectionParameters* client_connection_parameters,
      bool is_local_device);

  // Checks if |bluetooth_adapter_| is disabled or not present and rejects the
  // connection request if so. Returns whether the request was rejected.
  bool CheckIfBluetoothAdapterDisabledOrNotPresent(
      ApiFunctionName api_fn_name,
      ClientConnectionParameters* client_connection_parameters);

  // Validates |device| and adds it to the |remote_device_cache_| if it is
  // valid. If it is not valid, the reason is provided as a return type, and the
  // device is not added to the cache.
  base::Optional<InvalidRemoteDeviceReason> AddDeviceToCacheIfPossible(
      ApiFunctionName api_fn_name,
      const multidevice::RemoteDevice& device);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  std::unique_ptr<TimerFactory> timer_factory_;
  std::unique_ptr<multidevice::RemoteDeviceCache> remote_device_cache_;
  std::unique_ptr<BleServiceDataHelper> ble_service_data_helper_;
  std::unique_ptr<BleConnectionManager> ble_connection_manager_;
  std::unique_ptr<PendingConnectionManager> pending_connection_manager_;
  std::unique_ptr<ActiveConnectionManager> active_connection_manager_;

  base::flat_map<ConnectionDetails,
                 std::vector<ConnectionRequestWaitingForDisconnection>>
      disconnecting_details_to_requests_map_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelImpl);
};

std::ostream& operator<<(std::ostream& stream,
                         const SecureChannelImpl::ApiFunctionName& role);

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_IMPL_H_
