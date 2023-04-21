// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/nearby_metrics_recorder.h"

namespace base {
class Clock;
class OneShotTimer;
}  // namespace base

namespace ash {

namespace device_sync {
class DeviceSyncClient;
}

namespace multidevice_setup {
class MultiDeviceSetupClient;
}

namespace secure_channel {

class SecureChannelClient;

// ConnectionManager implementation which utilizes SecureChannelClient to
// establish a connection to a multidevice host.
class ConnectionManagerImpl : public ConnectionManager,
                              public ConnectionAttempt::Delegate,
                              public ClientChannel::Observer {
 public:
  ConnectionManagerImpl(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      device_sync::DeviceSyncClient* device_sync_client,
      SecureChannelClient* secure_channel_client,
      const std::string& feature_name,
      std::unique_ptr<NearbyMetricsRecorder> metrics_recorder);
  ~ConnectionManagerImpl() override;

  // ConnectionManager:
  ConnectionManager::Status GetStatus() const override;
  void AttemptNearbyConnection() override;
  void Disconnect() override;
  void SendMessage(const std::string& payload) override;
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
          file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;
  void GetHostLastSeenTimestamp(
      base::OnceCallback<void(absl::optional<base::Time>)> callback) override;

 private:
  friend class ConnectionManagerImplTest;

  ConnectionManagerImpl(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      device_sync::DeviceSyncClient* device_sync_client,
      SecureChannelClient* secure_channel_client,
      std::unique_ptr<base::OneShotTimer> timer,
      const std::string& feature_name,
      std::unique_ptr<NearbyMetricsRecorder> metrics_recorder,
      base::Clock* clock);

  // ConnectionAttempt::Delegate:
  void OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(std::unique_ptr<ClientChannel> channel) override;

  // ClientChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& payload) override;

  void OnConnectionTimeout();
  void TearDownConnection();

  void OnStatusChanged();
  void RecordMetrics();

  raw_ptr<multidevice_setup::MultiDeviceSetupClient, ExperimentalAsh>
      multidevice_setup_client_;
  raw_ptr<device_sync::DeviceSyncClient, ExperimentalAsh> device_sync_client_;
  raw_ptr<SecureChannelClient, ExperimentalAsh> secure_channel_client_;
  std::unique_ptr<ConnectionAttempt> connection_attempt_;
  std::unique_ptr<ClientChannel> channel_;
  std::unique_ptr<base::OneShotTimer> timer_;
  const std::string feature_name_;
  std::unique_ptr<NearbyMetricsRecorder> metrics_recorder_;
  Status last_status_;
  base::Time status_change_timestamp_;
  raw_ptr<base::Clock, ExperimentalAsh> clock_;
  base::WeakPtrFactory<ConnectionManagerImpl> weak_ptr_factory_{this};
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_IMPL_H_
