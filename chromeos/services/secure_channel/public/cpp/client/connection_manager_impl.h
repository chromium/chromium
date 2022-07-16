// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace chromeos {

namespace multidevice_setup {
class MultiDeviceSetupClient;
}  // namespace multidevice_setup

namespace secure_channel {

class SecureChannelClient;

// ConnectionManager implementation which utilizes SecureChannelClient to
// establish a connection to a multidevice host.
class ConnectionManagerImpl
    : public ConnectionManager,
      public secure_channel::ConnectionAttempt::Delegate,
      public secure_channel::ClientChannel::Observer {
 public:
  ConnectionManagerImpl(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      const std::string& feature_name,
      const std::string& metric_name_result,
      const std::string& metric_name_latency,
      const std::string& metric_name_duration);
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

 private:
  friend class ConnectionManagerImplTest;

  class MetricsRecorder : public ConnectionManager::Observer {
   public:
    MetricsRecorder(ConnectionManager* connection_manager,
                    base::Clock* clock,
                    const std::string& metric_name_result,
                    const std::string& metric_name_latency,
                    const std::string& metric_name_duration);
    ~MetricsRecorder() override;
    MetricsRecorder(const MetricsRecorder&) = delete;
    MetricsRecorder* operator=(const MetricsRecorder&) = delete;

    // ConnectionManager::Observer:
    void OnConnectionStatusChanged() override;

   private:
    ConnectionManager* connection_manager_;
    ConnectionManager::Status status_;
    base::Clock* clock_;
    base::Time status_change_timestamp_;
    const std::string metric_name_result_;
    const std::string metric_name_latency_;
    const std::string metric_name_duration_;
  };

  ConnectionManagerImpl(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      std::unique_ptr<base::OneShotTimer> timer,
      const std::string& feature_name,
      const std::string& metrics_name_result,
      const std::string& metrics_name_latency,
      const std::string& metrics_name_duration,
      base::Clock* clock);

  // secure_channel::ConnectionAttempt::Delegate:
  void OnConnectionAttemptFailure(
      secure_channel::mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(
      std::unique_ptr<secure_channel::ClientChannel> channel) override;

  // secure_channel::ClientChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& payload) override;

  void OnConnectionTimeout();
  void TearDownConnection();

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  device_sync::DeviceSyncClient* device_sync_client_;
  secure_channel::SecureChannelClient* secure_channel_client_;
  std::unique_ptr<chromeos::secure_channel::ConnectionAttempt>
      connection_attempt_;
  std::unique_ptr<chromeos::secure_channel::ClientChannel> channel_;
  std::unique_ptr<base::OneShotTimer> timer_;
  const std::string feature_name_;
  std::unique_ptr<MetricsRecorder> metrics_recorder_;
  base::WeakPtrFactory<ConnectionManagerImpl> weak_ptr_factory_{this};
};

}  // namespace secure_channel
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace ash {
namespace secure_channel {
using ::chromeos::secure_channel::ConnectionManagerImpl;
}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_IMPL_H_
