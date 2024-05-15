// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_HOST_CONNECTION_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_HOST_CONNECTION_H_

#include <memory>

#include "base/functional/callback.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/tether_host.h"

namespace ash::tether {

class HostConnection {
 public:
  class PayloadListener {
   public:
    virtual ~PayloadListener() = default;
    virtual void OnMessageReceived(std::unique_ptr<MessageWrapper> message) = 0;
  };

  using OnDisconnectionCallback = base::OnceClosure;

  class Factory {
   public:
    enum class ConnectionPriority {
      kLow,
      kMedium,
      kHigh,
    };

    using OnConnectionCreatedCallback =
        base::OnceCallback<void(std::unique_ptr<HostConnection>)>;

    virtual ~Factory() = default;
    virtual void ScanForTetherHostAndCreateConnection(
        const std::string& device_id,
        ConnectionPriority connection_priority,
        raw_ptr<PayloadListener> payload_listener,
        OnDisconnectionCallback on_disconnection,
        OnConnectionCreatedCallback on_connection_created) = 0;
    virtual void Create(const TetherHost& tether_host,
                        ConnectionPriority connection_priority,
                        raw_ptr<PayloadListener> payload_listener,
                        OnDisconnectionCallback on_disconnection,
                        OnConnectionCreatedCallback on_connection_created) = 0;
  };

  using OnMessageSentCallback = base::OnceClosure;

  HostConnection(raw_ptr<PayloadListener> payload_listener,
                 OnDisconnectionCallback on_disconnection);
  virtual ~HostConnection();
  HostConnection(const HostConnection&) = delete;
  HostConnection& operator=(const HostConnection&) = delete;

  virtual void SendMessage(std::unique_ptr<MessageWrapper> message,
                           OnMessageSentCallback on_message_sent_callback) = 0;

 protected:
  void ParseMessageAndNotifyListener(const std::string& payload);

  raw_ptr<PayloadListener> payload_listener_;
  OnDisconnectionCallback on_disconnection_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_HOST_CONNECTION_H_
