// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_SECURE_CHANNEL_HOST_CONNECTION_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_SECURE_CHANNEL_HOST_CONNECTION_H_

#include "base/uuid.h"
#include "chromeos/ash/components/tether/host_connection.h"
#include "chromeos/ash/components/tether/tether_host_fetcher.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace ash::tether {

class SecureChannelHostConnection
    : public HostConnection,
      public secure_channel::ClientChannel::Observer {
 public:
  class Factory : public HostConnection::Factory {
   public:
    Factory(raw_ptr<device_sync::DeviceSyncClient> device_sync_client,
            raw_ptr<secure_channel::SecureChannelClient> secure_channel_client,
            raw_ptr<TetherHostFetcher> tether_host_fetcher);
    ~Factory() override;
    Factory(Factory&) = delete;
    Factory& operator=(Factory&) = delete;

    // HostConnection::Factory:
    void ScanForTetherHostAndCreateConnection(
        const std::string& device_id,
        ConnectionPriority connection_priority,
        raw_ptr<HostConnection::PayloadListener> payload_listener,
        HostConnection::OnDisconnectionCallback on_disconnection,
        HostConnection::Factory::OnConnectionCreatedCallback
            on_connection_created) override;
    void Create(const TetherHost& tether_host,
                ConnectionPriority connection_priority,
                raw_ptr<HostConnection::PayloadListener> payload_listener,
                HostConnection::OnDisconnectionCallback on_disconnection,
                HostConnection::Factory::OnConnectionCreatedCallback
                    on_connection_created) override;

   private:
    class ConnectionAttemptDelegateImpl
        : public secure_channel::ConnectionAttempt::Delegate {
     public:
      using OnConnectionAttemptDelegateFinishedCallback =
          base::OnceCallback<void(
              std::unique_ptr<HostConnection> host_connection)>;

      ConnectionAttemptDelegateImpl(
          const multidevice::RemoteDeviceRef& local_device,
          const multidevice::RemoteDeviceRef& remote_device,
          secure_channel::SecureChannelClient* secure_channel_client,
          HostConnection::PayloadListener* payload_listener,
          HostConnection::OnDisconnectionCallback on_disconnection);
      ~ConnectionAttemptDelegateImpl() override;

      void Start(
          secure_channel::ConnectionPriority connection_priority,
          OnConnectionAttemptDelegateFinishedCallback on_connection_created);

     private:
      // secure_channel::ConnectionAttempt::Delegate:
      void OnConnectionAttemptFailure(
          secure_channel::mojom::ConnectionAttemptFailureReason reason)
          override;
      void OnConnection(
          std::unique_ptr<secure_channel::ClientChannel> channel) override;

      const multidevice::RemoteDeviceRef local_device_;
      const multidevice::RemoteDeviceRef remote_device_;
      raw_ptr<HostConnection::PayloadListener> payload_listener_;
      HostConnection::OnDisconnectionCallback on_disconnection_;
      std::unique_ptr<secure_channel::ConnectionAttempt> connection_attempt_;
      raw_ptr<secure_channel::SecureChannelClient> secure_channel_client_;
      OnConnectionAttemptDelegateFinishedCallback
          on_connection_attempt_delegate_finished_;
    };

    void OnConnectionAttemptFinished(
        base::Uuid connection_attempt_id,
        HostConnection::Factory::OnConnectionCreatedCallback
            on_connection_created,
        std::unique_ptr<HostConnection> host_connection);

    std::map<base::Uuid, std::unique_ptr<ConnectionAttemptDelegateImpl>>
        active_connection_attempts_;
    raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;
    raw_ptr<secure_channel::SecureChannelClient> secure_channel_client_;
    raw_ptr<TetherHostFetcher> tether_host_fetcher_;

    base::WeakPtrFactory<Factory> weak_ptr_factory_{this};
  };

  ~SecureChannelHostConnection() override;
  SecureChannelHostConnection(SecureChannelHostConnection&) = delete;
  SecureChannelHostConnection& operator=(SecureChannelHostConnection&) = delete;

  // HostConnection:
  void SendMessage(std::unique_ptr<MessageWrapper> message,
                   OnMessageSentCallback on_message_sent_callback) override;

 protected:
  // secure_channel::ClientChannel::Observer:
  void OnMessageReceived(const std::string& message) override;
  void OnDisconnected() override;

 private:
  SecureChannelHostConnection(
      raw_ptr<HostConnection::PayloadListener> listener,
      HostConnection::OnDisconnectionCallback on_disconnection,
      std::unique_ptr<secure_channel::ClientChannel> client_channel);

  std::unique_ptr<secure_channel::ClientChannel> client_channel_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_SECURE_CHANNEL_HOST_CONNECTION_H_
