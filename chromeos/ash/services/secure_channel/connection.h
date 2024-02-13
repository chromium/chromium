// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_H_

#include <memory>
#include <optional>
#include <ostream>

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom-forward.h"

namespace ash::secure_channel {

class ConnectionObserver;
class NearbyConnectionObserver;
class WireMessage;

// Base class representing a connection with a remote device, which is a
// persistent bidirectional channel for sending and receiving wire messages.
class Connection {
 public:
  enum class Status {
    DISCONNECTED,
    IN_PROGRESS,
    CONNECTED,
  };

  // Constructs a connection to the given |remote_device|.
  explicit Connection(multidevice::RemoteDeviceRef remote_device);

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  virtual ~Connection();

  // Returns true iff the connection's status is CONNECTED.
  bool IsConnected() const;

  // Returns true iff the connection is currently sending a message.
  bool is_sending_message() const { return is_sending_message_; }

  // Sends a message to the remote device.
  // |OnSendCompleted()| will be called for all observers upon completion with
  // either success or failure.
  void SendMessage(std::unique_ptr<WireMessage> message);

  // Registers |payload_files| to receive an incoming file transfer with
  // the given |payload_id|. |registration_result_callback| will return true
  // if the file was successfully registered, or false if the registration
  // failed or if this operation is not supported by the connection type.
  // Callers can listen to progress information about the transfer through the
  // |file_transfer_update_callback| if the registration was successful.
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback);

  virtual void AddObserver(ConnectionObserver* observer);
  virtual void RemoveObserver(ConnectionObserver* observer);

  virtual void AddNearbyConnectionObserver(
      NearbyConnectionObserver* nearby_connection_observer);
  virtual void RemoveNearbyConnectionObserver(
      NearbyConnectionObserver* nearby_connection_observer);

  multidevice::RemoteDeviceRef remote_device() const { return remote_device_; }

  // Returns the RSSI of the connection; if no derived class overrides this
  // function, std::nullopt is returned.
  virtual void GetConnectionRssi(
      base::OnceCallback<void(std::optional<int32_t>)> callback);

  // Abstract methods that subclasses should implement:

  // Attempts to connect to the remote device if not already connected.
  virtual void Connect() = 0;

  // Disconnects from the remote device.
  virtual void Disconnect() = 0;

  // The bluetooth address of the connected device.
  virtual std::string GetDeviceAddress() = 0;

  Status status() const { return status_; }

 protected:
  // Sets the connection's status to |status|. If this is different from the
  // previous status, notifies observers of the change in status.
  // Virtual for testing.
  virtual void SetStatus(Status status);

  virtual void SetNearbyConnectionSubStatus(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result);

  // Called after attempting to send bytes over the connection, whether the
  // message was successfully sent or not.
  // Virtual for testing.
  virtual void OnDidSendMessage(const WireMessage& message, bool success);

  // Called when bytes are read from the connection. There should not be a send
  // in progress when this function is called.
  // Virtual for testing.
  virtual void OnBytesReceived(const std::string& bytes);

  // Sends bytes over the connection. The implementing class should call
  // OnDidSendMessage() once the send succeeds or fails. At most one send will
  // be
  // in progress.
  virtual void SendMessageImpl(std::unique_ptr<WireMessage> message) = 0;

  // Registers the payload file over the connection. The implementing class
  // should invoke |registration_result_callback| with the registration result,
  // or false if the operation is not supported.
  virtual void RegisterPayloadFileImpl(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) = 0;

  // Deserializes the |recieved_bytes_| and returns the resulting WireMessage,
  // or NULL if the message is malformed. Sets |is_incomplete_message| to true
  // if the |serialized_message| does not have enough data to parse the header,
  // or if the message length encoded in the message header exceeds the size of
  // the |serialized_message|. Exposed for testing.
  virtual std::unique_ptr<WireMessage> DeserializeWireMessage(
      bool* is_incomplete_message);

  // Returns a string describing the associated device for logging purposes.
  std::string GetDeviceInfoLogString();

 private:
  // The remote device corresponding to this connection.
  const multidevice::RemoteDeviceRef remote_device_;

  // The current status of the connection.
  Status status_;

  // The registered observers of the connection.
  base::ObserverList<ConnectionObserver>::Unchecked observers_;

  base::ObserverList<NearbyConnectionObserver>::Unchecked
      nearby_connection_state_observers_;

  // A temporary buffer storing bytes received before a received message can be
  // fully constructed.
  std::string received_bytes_;

  // Whether a message is currently in the process of being sent.
  bool is_sending_message_;
};

std::ostream& operator<<(std::ostream& stream,
                         const Connection::Status& status);

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_H_
