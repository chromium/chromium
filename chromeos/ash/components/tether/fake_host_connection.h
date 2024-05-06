// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_HOST_CONNECTION_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_HOST_CONNECTION_H_

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/tether/host_connection.h"

namespace ash::tether {

class FakeHostConnection : public HostConnection {
 public:
  class Factory : public HostConnection::Factory {
   public:
    Factory();
    ~Factory() override;

    // HostConnection::Factory:
    void ScanForTetherHostAndCreateConnection(
        const std::string& device_id,
        ConnectionPriority connection_priority,
        raw_ptr<PayloadListener> payload_listener,
        OnDisconnectionCallback on_disconnection,
        OnConnectionCreatedCallback on_connection_created) override;
    void Create(const TetherHost& tether_host,
                ConnectionPriority connection_priority,
                raw_ptr<PayloadListener> payload_listener,
                OnDisconnectionCallback on_disconnection,
                OnConnectionCreatedCallback on_connection_created) override;

    // Creates a new [FakeHostConnection] and sets it as the connection to be
    // returned when [tether_host] is requested.
    FakeHostConnection* SetupConnectionAttempt(const TetherHost& tether_host);

    // Setup the connection attempt, passing in a [host_connection] which will
    // be returned when [device_id] is requested. Pass in [nullptr] to fail the
    // connection attempt.
    void SetupConnectionAttempt(const std::string& device_id,
                                FakeHostConnection* host_connection);

    FakeHostConnection* GetPendingConnectionAttempt(
        const std::string& device_id);

   private:
    base::flat_map<std::string, FakeHostConnection*>
        pending_connection_attempts_;
  };

  explicit FakeHostConnection();
  ~FakeHostConnection() override;

  // HostConnection:
  void SendMessage(std::unique_ptr<MessageWrapper> message,
                   OnMessageSentCallback on_message_sent_callback) override;

  void ReceiveMessage(std::unique_ptr<MessageWrapper> message);
  void Close();
  void FinishSendingMessages();

  void set_on_disconnection(base::OnceClosure on_disconnection) {
    on_disconnection_ = std::move(on_disconnection);
  }

  void set_payload_listener(HostConnection::PayloadListener* payload_listener) {
    payload_listener_ = payload_listener;
  }

  const std::vector<
      std::pair<std::unique_ptr<MessageWrapper>, OnMessageSentCallback>>&
  sent_messages() {
    return sent_messages_;
  }

 private:
  std::vector<std::pair<std::unique_ptr<MessageWrapper>, OnMessageSentCallback>>
      sent_messages_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_HOST_CONNECTION_H_
