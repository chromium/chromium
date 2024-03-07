// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_H_

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/authenticator.h"
#include "chromeos/ash/services/secure_channel/connection.h"
#include "chromeos/ash/services/secure_channel/connection_observer.h"
#include "chromeos/ash/services/secure_channel/device_to_device_authenticator.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom-forward.h"
#include "chromeos/ash/services/secure_channel/secure_context.h"

namespace ash::secure_channel {

// An authenticated bi-directional channel for exchanging messages with remote
// devices. |SecureChannel| manages a |Connection| by initializing it and
// authenticating it via a security handshake once the connection has occurred.
// Once the channel has been authenticated, messages sent are automatically
// encrypted and messages received are automatically decrypted.
class SecureChannel : public ConnectionObserver,
                      public NearbyConnectionObserver,
                      public AuthenticatorObserver {
 public:
  // Enumeration of possible states of connecting to a remote device.
  //   DISCONNECTED: There is no connection to the device, nor is there a
  //       pending connection attempt.
  //   CONNECTING: There is an ongoing connection attempt.
  //   CONNECTED: There is a Bluetooth connection to the device, but the
  //       connection has not yet been authenticated.
  //   AUTHENTICATING: There is an active connection that is currently in the
  //       process of authenticating via a 3-message authentication handshake.
  //   AUTHENTICATED: The connection has been authenticated, and arbitrary
  //       messages can be sent/received to/from the device.
  //   DISCONNECTING: The connection has started disconnecting but has not yet
  //       finished.
  enum class Status {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AUTHENTICATING,
    AUTHENTICATED,
    DISCONNECTING
  };

  static std::string StatusToString(const Status& status);

  class Observer {
   public:
    virtual void OnSecureChannelStatusChanged(SecureChannel* secure_channel,
                                              const Status& old_status,
                                              const Status& new_status) {}

    virtual void OnMessageReceived(SecureChannel* secure_channel,
                                   const std::string& feature,
                                   const std::string& payload) {}

    // Called when a message has been sent successfully; |sequence_number|
    // corresponds to the value returned by an earlier call to SendMessage().
    virtual void OnMessageSent(SecureChannel* secure_channel,
                               int sequence_number) {}

    virtual void OnNearbyConnectionStateChanged(
        SecureChannel* secure_channel,
        mojom::NearbyConnectionStep step,
        mojom::NearbyConnectionStepResult result) {}

    virtual void OnSecureChannelAuthenticationStateChanged(
        SecureChannel* secure_channel,
        mojom::SecureChannelState secure_channel_state) {}
  };

  class Factory {
   public:
    static std::unique_ptr<SecureChannel> Create(
        std::unique_ptr<Connection> connection);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<SecureChannel> CreateInstance(
        std::unique_ptr<Connection> connection) = 0;

   private:
    static Factory* factory_instance_;
  };

  SecureChannel(const SecureChannel&) = delete;
  SecureChannel& operator=(const SecureChannel&) = delete;

  ~SecureChannel() override;

  virtual void Initialize();

  // Sends a message over the connection and returns a sequence number. If the
  // message is successfully sent, observers will be notified that the message
  // has been sent and will be provided this sequence number.
  virtual int SendMessage(const std::string& feature,
                          const std::string& payload);

  // Registers |payload_files| to receive an incoming file transfer with
  // the given |payload_id|. |registration_result_callback| will return true
  // if the file was successfully registered, or false if the registration
  // failed or if this operation is not supported by the connection type.
  // Callers can listen to progress information about the transfer through the
  // |file_transfer_update_callback| if the registration was successful.
  virtual void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback);

  virtual void Disconnect();

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Returns the RSSI of the connection; if no derived class overrides this
  // function, std::nullopt is returned.
  virtual void GetConnectionRssi(
      base::OnceCallback<void(std::optional<int32_t>)> callback);

  // The |responder_auth| message. Returns null if |secure_context_| is null or
  // status() != AUTHENTICATED.
  virtual std::optional<std::string> GetChannelBindingData();

  Status status() const { return status_; }

  // ConnectionObserver:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override;
  void OnMessageReceived(const Connection& connection,
                         const WireMessage& wire_message) override;
  void OnSendCompleted(const Connection& connection,
                       const WireMessage& wire_message,
                       bool success) override;

  // NearbyConnectionObserver:
  void OnNearbyConnectionStateChagned(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) override;

  // AuthenticatorObserver:
  void OnAuthenticationStateChanged(
      mojom::SecureChannelState secure_channel_state) override;

 protected:
  explicit SecureChannel(std::unique_ptr<Connection> connection);

  Status status_;

 private:
  friend class SecureChannelConnectionTest;

  // Message waiting to be sent. Note that this is *not* the message that will
  // end up being sent over the wire; before that can be done, the payload must
  // be encrypted.
  struct PendingMessage {
    PendingMessage(const std::string& feature,
                   const std::string& payload,
                   int sequence_number);
    virtual ~PendingMessage();

    const std::string feature;
    const std::string payload;
    const int sequence_number;
  };

  void TransitionToStatus(const Status& new_status);
  void Authenticate();
  void ProcessMessageQueue();
  void OnMessageEncoded(const std::string& feature,
                        int sequence_number,
                        const std::string& encoded_message);
  void OnMessageDecoded(const std::string& feature,
                        const std::string& decoded_message);
  void OnAuthenticationResult(Authenticator::Result result,
                              std::unique_ptr<SecureContext> secure_context);

  std::unique_ptr<Connection> connection_;
  std::unique_ptr<Authenticator> authenticator_;
  std::unique_ptr<SecureContext> secure_context_;
  base::queue<std::unique_ptr<PendingMessage>> queued_messages_;
  std::unique_ptr<PendingMessage> pending_message_;
  int next_sequence_number_ = 0;
  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observer_list_;
  base::WeakPtrFactory<SecureChannel> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_H_
