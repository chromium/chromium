// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_CONNECTION_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_CONNECTION_MANAGER_H_

#include <ostream>

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace chromeos {
namespace phonehub {

// Responsible for creating and maintaining a connection to the user's phone.
class ConnectionManager {
 public:
  enum class Status {
    // Disconnected from the host phone.
    kDisconnected,

    // Initiating a connection to the host phone.
    kConnecting,

    // Connected to the host phone.
    kConnected,
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called the status has changed; use GetStatus() to get the new status.
    virtual void OnConnectionStatusChanged() {}

    // Called when a payload message has been received.
    virtual void OnMessageReceived(const std::string& payload) {}
  };

  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager* operator=(const ConnectionManager&) = delete;
  virtual ~ConnectionManager();

  virtual Status GetStatus() const = 0;

  // Initiates a connection to the host phone. The local device
  // (e.g. this chromebook) must have Bluetooth enabled in order to bootstrap
  // the connection.
  virtual void AttemptConnection() = 0;

  // Sends a message with the specified |payload|.
  virtual void SendMessage(const std::string& payload) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ConnectionManager();

  void NotifyStatusChanged();
  void NotifyMessageReceived(const std::string& payload);

 private:
  base::ObserverList<Observer> observer_list_;
};

std::ostream& operator<<(std::ostream& stream,
                         ConnectionManager::Status status);

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_CONNECTION_MANAGER_H_
