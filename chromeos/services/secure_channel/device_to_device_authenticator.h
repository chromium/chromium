// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_AUTHENTICATOR_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_AUTHENTICATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/authenticator.h"
#include "chromeos/services/secure_channel/connection.h"
#include "chromeos/services/secure_channel/connection_observer.h"
#include "chromeos/services/secure_channel/device_to_device_initiator_helper.h"
#include "chromeos/services/secure_channel/session_keys.h"

namespace base {
class OneShotTimer;
}

namespace chromeos {

namespace multidevice {
class SecureMessageDelegate;
}  // namespace multidevice

namespace secure_channel {

// Authenticator implementation using the "device to device" protocol, which is
// in turn built on top of the SecureMessage library.
// This protocol contains the following steps (local device is the initiator):
//   1. Both initiator and responder devices generate a temporary key pair for
//      the session.
//   2. Initiator sends [Hello] message to responder device, which contains the
//      initiator's session public key.
//   3. Responder responds with a [Responder Auth] message, containing its
//      session public key and data that allows the initiator to assert the
//      identity of the responder.
//   4. Initiator sends [Initiator Auth] message, containing data allowing the
//      responder to assert the identity of the initiator.
//   5. Both devices derive a symmetric key by running a key agreement protocol
//      session public keys they obtain from from the messages above. This
//      symmetric key is used in the subsequent SecureContext.
// The authentication protocol fails if any of the steps above fail.
// This protocol requires exclusive use of the connection. No other message
// should be sent or received while authentication is in progress.
class DeviceToDeviceAuthenticator : public Authenticator,
                                    public ConnectionObserver {
 public:
  class Factory {
   public:
    static std::unique_ptr<Authenticator> NewInstance(
        Connection* connection,
        const std::string& account_id,
        std::unique_ptr<multidevice::SecureMessageDelegate>
            secure_message_delegate);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<Authenticator> BuildInstance(
        Connection* connection,
        const std::string& account_id,
        std::unique_ptr<multidevice::SecureMessageDelegate>
            secure_message_delegate);

   private:
    static Factory* factory_instance_;
  };

  // Creates the instance:
  // |connection|: The connection to the remote device, which must be in a
  //     connected state. Not owned.
  // |account_id|: The canonical account id of the user who is the owner of both
  //     the local and remote devices.
  // |secure_message_delegate|: Handles the SecureMessage crypto operations.
  DeviceToDeviceAuthenticator(
      Connection* connection,
      const std::string& account_id,
      std::unique_ptr<multidevice::SecureMessageDelegate>
          secure_message_delegate);

  ~DeviceToDeviceAuthenticator() override;

  // Authenticator:
  void Authenticate(const AuthenticationCallback& callback) override;

 protected:
  // Creates a base::OneShotTimer instance. Exposed for testing.
  virtual std::unique_ptr<base::OneShotTimer> CreateTimer();

 private:
  // The current state of the authentication flow.
  enum class State {
    NOT_STARTED,
    GENERATING_SESSION_KEYS,
    SENDING_HELLO,
    SENT_HELLO,
    RECEIVED_RESPONDER_AUTH,
    VALIDATED_RESPONDER_AUTH,
    SENT_INITIATOR_AUTH,
    AUTHENTICATION_SUCCESS,
    AUTHENTICATION_FAILURE,
  };

  // Callback when the session key pair is generated.
  void OnKeyPairGenerated(const std::string& public_key,
                          const std::string& private_key);

  // Callback when [Hello] is created.
  void OnHelloMessageCreated(const std::string& message);

  // Callback when waiting for [Remote Auth] times out.
  void OnResponderAuthTimedOut();

  // Callback for validating the received [Remote Auth].
  void OnResponderAuthValidated(bool validated,
                                const SessionKeys& session_keys);

  // Callback when [Initiator Auth] is created.
  void OnInitiatorAuthCreated(const std::string& message);

  // Callback when the session symmetric key is derived.
  void OnKeyDerived(const std::string& session_symmetric_key);

  // Called when the authentication flow fails, and logs |error_message|. The
  // overloaded version specifies the Result to be reported;
  // otherwise, a FAILURE result will be reported.
  void Fail(const std::string& error_message);
  void Fail(const std::string& error_message, Result result);

  // Called when the authentication flow succeeds.
  void Succeed();

  // ConnectionObserver:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override;
  void OnMessageReceived(const Connection& connection,
                         const WireMessage& message) override;
  void OnSendCompleted(const Connection& connection,
                       const WireMessage& message,
                       bool success) override;

  // The connection to the remote device. It is expected to be in the CONNECTED
  // state at all times during authentication.
  // Not owned, and must outlive this instance.
  Connection* const connection_;

  // The account id of the user who owns the local and remote devices. This is
  // normally an email address, and should be canonicalized.
  const std::string account_id_;

  // Handles SecureMessage crypto operations.
  std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate_;

  // Performs authentication handshake.
  std::unique_ptr<DeviceToDeviceInitiatorHelper> helper_;

  // The current state in the authentication flow.
  State state_;

  // Callback to invoke when authentication completes.
  AuthenticationCallback callback_;

  // Used for timing out when waiting for [Remote Auth] from the remote device.
  std::unique_ptr<base::OneShotTimer> timer_;

  // The bytes of the [Hello] message sent to the remote device.
  std::string hello_message_;

  // The bytes of the [Responder Auth] message received from the remote device.
  std::string responder_auth_message_;

  // The private key generated for the session.
  std::string local_session_private_key_;

  // The derived symmetric keys for the session.
  SessionKeys session_keys_;

  base::WeakPtrFactory<DeviceToDeviceAuthenticator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceToDeviceAuthenticator);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_AUTHENTICATOR_H_
