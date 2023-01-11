// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_RESPONDER_OPERATIONS_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_RESPONDER_OPERATIONS_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace ash {

namespace multidevice {
class SecureMessageDelegate;
}

namespace secure_channel {

class SessionKeys;

// Utility class containing operations in the DeviceToDevice protocol that the
// initiator needs to perform. For Smart Lock, in which a phone unlocks a
// laptop, the responder is the phone. Because the responder side of this
// protocol does not run in Chrome, this class is implemented solely for
// testing purposes.
//
// All operations are asynchronous because we use the SecureMessageDelegate for
// crypto operations, whose implementation may be asynchronous.
//
// In the DeviceToDevice protocol, the responder parses two messages received
// from the initiator and sends one message:
//   1. Parse [Hello] Message
//      This message contains the initiator's session public key and is signed
//      by the long term symmetric key.
//   2. Send [Responder Auth] Message
//      This message contains the responder's session public key, and allows the
//      initiator to authenticate the responder. After both sides have each
//      other's public keys, they can derive a symmetric key for the session.
//   3. Parse [Initiator Auth] Message
//      This message allows the responder to authenticate the initiator.
class DeviceToDeviceResponderOperations {
 public:
  // Callback for operations that create a message. Invoked with the serialized
  // SecureMessage upon success or the empty string upon failure.
  typedef base::OnceCallback<void(const std::string&)> MessageCallback;

  // Callback for operations that validates a message.
  typedef base::OnceCallback<void(bool)> ValidationCallback;

  // Callback for ValidateHelloMessage. The first argument will be called with
  // the validation outcome. If validation succeeded, then the second argument
  // will contain the initiator's public key.
  typedef base::OnceCallback<void(bool, const std::string&)>
      ValidateHelloCallback;

  DeviceToDeviceResponderOperations() = delete;
  DeviceToDeviceResponderOperations(const DeviceToDeviceResponderOperations&) =
      delete;
  DeviceToDeviceResponderOperations& operator=(
      const DeviceToDeviceResponderOperations&) = delete;

  // Validates that the [Hello] message, received from the initiator,
  // is properly signed and encrypted.
  // |hello_message|: The bytes of the [Hello] message to validate.
  // |persistent_symmetric_key|: The long-term symmetric key that is shared by
  //     the initiator and responder.
  // |secure_message_delegate|: Delegate for SecureMessage operations. This
  //     instance is not owned, and must live until after |callback| is invoked.
  // |callback|: Invoked upon operation completion with whether
  //     |responder_auth_message| is validated successfully and the initiator's
  //     public key.
  static void ValidateHelloMessage(
      const std::string& hello_message,
      const std::string& persistent_symmetric_key,
      multidevice::SecureMessageDelegate* secure_message_delegate,
      ValidateHelloCallback callback);

  // Creates the [Responder Auth] message:
  // |hello_message|: The initial [Hello] message that was sent, which is used
  //     in the signature calculation.
  // |session_public_key|: This session public key will be stored in plaintext
  //     to be read by the initiator.
  // |session_private_key|: The session private key is used in conjunction with
  //     the initiator's public key to derive the session symmetric key.
  // |persistent_private_key|: The long-term private key possessed by the
  //     responder device.
  // |persistent_symmetric_key|: The long-term symmetric key that is shared by
  //     the initiator and responder.
  // |secure_message_delegate|: Delegate for SecureMessage operations. This
  //     instance is not owned, and must live until after |callback| is invoked.
  // |callback|: Invoked upon operation completion with the serialized message
  //     or an empty string.
  static void CreateResponderAuthMessage(
      const std::string& hello_message,
      const std::string& session_public_key,
      const std::string& session_private_key,
      const std::string& persistent_private_key,
      const std::string& persistent_symmetric_key,
      multidevice::SecureMessageDelegate* secure_message_delegate,
      MessageCallback callback);

  // Validates that the [Initiator Auth] message, received from the initiator,
  // is properly signed and encrypted.
  // |initiator_auth_message|: The bytes of the [Local Auth] message to
  // validate.
  // |session_keys|: The derived symmetric keys used just for the session.
  // |persistent_symmetric_key|: The long-term symmetric key that is shared by
  //     the initiator and responder.
  // |secure_message_delegate|: Delegate for SecureMessage operations. This
  //     instance is not owned, and must live until after |callback| is invoked.
  // |callback|: Invoked upon operation completion with whether
  //     |responder_auth_message| is validated successfully.
  static void ValidateInitiatorAuthMessage(
      const std::string& initiator_auth_message,
      const SessionKeys& session_keys,
      const std::string& persistent_symmetric_key,
      const std::string& responder_auth_message,
      multidevice::SecureMessageDelegate* secure_message_delegate,
      ValidationCallback callback);
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_RESPONDER_OPERATIONS_H_
