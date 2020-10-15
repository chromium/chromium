// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_CONNECTION_MANAGER_IMPL_H_

#include <memory>

#include "base/optional.h"
#include "chromeos/components/phonehub/connection_manager.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace multidevice_setup {
class MultiDeviceSetupClient;
}  // namespace multidevice_setup

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace phonehub {

// ConnectionManager implementation which utilizes SecureChannelClient to
// establish a connection to a host phone.
class ConnectionManagerImpl
    : public ConnectionManager,
      public chromeos::secure_channel::ConnectionAttempt::Delegate,
      public chromeos::secure_channel::ClientChannel::Observer {
 public:
  ConnectionManagerImpl(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      device_sync::DeviceSyncClient* device_sync_client,
      chromeos::secure_channel::SecureChannelClient* secure_channel_client);
  ~ConnectionManagerImpl() override;

  // ConnectionManager:
  ConnectionManager::Status GetStatus() const override;
  void AttemptConnection() override;
  void SendMessage(const std::string& payload) override;

 private:
  // chromeos::secure_channel::ConnectionAttempt::Delegate:
  void OnConnectionAttemptFailure(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason reason)
      override;
  void OnConnection(std::unique_ptr<chromeos::secure_channel::ClientChannel>
                        channel) override;

  // chromeos::secure_channel::ClientChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& payload) override;

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;

  device_sync::DeviceSyncClient* device_sync_client_;

  // The entrypoint to the SecureChannel API.
  chromeos::secure_channel::SecureChannelClient* secure_channel_client_;

  std::unique_ptr<chromeos::secure_channel::ConnectionAttempt>
      connection_attempt_;

  std::unique_ptr<chromeos::secure_channel::ClientChannel> channel_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_CONNECTION_MANAGER_IMPL_H_
