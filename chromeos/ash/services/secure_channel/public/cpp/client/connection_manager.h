// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_H_

#include <ostream>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

// Responsible for creating and maintaining a connection to the user's
// multidevice host.
class ConnectionManager {
 public:
  enum class Status {
    // Disconnected from the multidevice host.
    kDisconnected,

    // Initiating a connection to the multidevice host.
    kConnecting,

    // Connected to the multidevice host.
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

  // Initiates a connection to the multidevice host with medium priority using
  // nearby. The local device (e.g. this chromebook) must have Bluetooth
  // enabled in order to bootstrap the connection. Return true if the attempt is
  // Initiated.
  virtual bool AttemptNearbyConnection() = 0;

  // Cancels an ongoing connection attempt. Is a no-op is there is currently no
  // connection attempt.
  virtual void Disconnect() = 0;

  // Sends a message with the specified |payload|.
  virtual void SendMessage(const std::string& payload) = 0;

  // Registers |payload_files| to receive an incoming file transfer with
  // the given |payload_id|. |registration_result_callback| will return true
  // if the file was successfully registered, or false if the registration
  // failed or if this operation is not supported by the connection type.
  // Callers can listen to progress information about the transfer through the
  // |file_transfer_update_callback| if the registration was successful.
  virtual void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
          file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) = 0;

  virtual void GetHostLastSeenTimestamp(
      base::OnceCallback<void(std::optional<base::Time>)> callback) = 0;

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

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_H_
