// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_AUTHENTICATED_CHANNEL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_AUTHENTICATED_CHANNEL_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

// A full-duplex communication channel which is guaranteed to be authenticated
// (i.e., the two sides of the channel both belong to the same underlying user).
// All messages sent and received over the channel are encrypted.
class AuthenticatedChannel {
 public:
  class Observer {
   public:
    virtual ~Observer();
    virtual void OnDisconnected() = 0;
    virtual void OnMessageReceived(const std::string& feature,
                                   const std::string& payload) = 0;
    virtual void OnNearbyConnectionStateChanged(
        mojom::NearbyConnectionStep step,
        mojom::NearbyConnectionStepResult result) = 0;
  };

  AuthenticatedChannel(const AuthenticatedChannel&) = delete;
  AuthenticatedChannel& operator=(const AuthenticatedChannel&) = delete;

  virtual ~AuthenticatedChannel();

  virtual void GetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) = 0;

  // Sends a message with the specified |feature| and |payload|. Once the
  // message has been sent, |on_sent_callback| will be invoked. Returns whether
  // this AuthenticatedChannel was able to start sending the message; this
  // function only fails if the underlying connection has been disconnected.
  bool SendMessage(const std::string& feature,
                   const std::string& payload,
                   base::OnceClosure on_sent_callback);

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

  // Disconnects this channel. Note that disconnection is an asynchronous
  // operation; observers will be notified when disconnection completes via the
  // OnDisconnected() callback.
  void Disconnect();

  bool is_disconnected() const { return is_disconnected_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  AuthenticatedChannel();

  // Performs the actual logic of sending the message. By the time this function
  // is called, it has already been confirmed that the channel has not been
  // disconnected.
  virtual void PerformSendMessage(const std::string& feature,
                                  const std::string& payload,
                                  base::OnceClosure on_sent_callback) = 0;

  // Performs the actual logic of registering payload files. By the time this
  // function is called, it has already been confirmed that the channel has not
  // been disconnected.
  virtual void PerformRegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) = 0;

  // Performs the actual logic of disconnecting. By the time this function is
  // called, it has already been confirmed that the channel is still indeed
  // connected.
  virtual void PerformDisconnection() = 0;

  void NotifyDisconnected();
  void NotifyMessageReceived(const std::string& feature,
                             const std::string& payload);
  void NotifyNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result);

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
  bool is_disconnected_ = false;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_AUTHENTICATED_CHANNEL_H_
