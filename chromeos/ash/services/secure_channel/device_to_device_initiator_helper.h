// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_INITIATOR_HELPER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_INITIATOR_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/secure_channel/session_keys.h"
#include "third_party/ukey2/proto/device_to_device_messages.pb.h"

namespace ash {

namespace multidevice {
class SecureMessageDelegate;
}

namespace secure_channel {

// Class containing operations in the DeviceToDevice protocol that the initiator
// needs to perform. This class is instantiable rather than being a utility
// class because it relies on a WeakPtrFactory to prevent referencing deleted
// memory.
//
// All operations are asynchronous because we use the SecureMessageDelegate for
// crypto operations, whose implementation may be asynchronous.
//
// In the DeviceToDevice protocol, the initiator needs to send two messages to
// the responder and parse one message from the responder:
//   1. Send [Hello] Message
//      This message contains a public key that the initiator generates for the
//      current session. This message is signed by the long term symmetric key.
//   2. Parse [Responder Auth] Message
//      The responder parses [Hello] and sends this message, which contains the
//      responder's session public key. This message also contains sufficient
//      information for the initiator to authenticate the responder.
//   3. Send [Initiator Auth] Message
//      After receiving the responder's session public key, the initiator crafts
//      and sends this message so the responder can authenticate the initiator.
class DeviceToDeviceInitiatorHelper {
 public:
  // Callback for operations that create a message. Invoked with the serialized
  // SecureMessage upon success or the empty string upon failure.
  typedef base::OnceCallback<void(const std::string&)> MessageCallback;

  // Callback for ValidateResponderAuthMessage. The first argument will be
  // called with the validation outcome. If validation succeeded, then the
  // second argument will contain the session symmetric key derived from the
  // [Responder Auth] message.
  typedef base::OnceCallback<void(bool, const SessionKeys&)>
      ValidateResponderAuthCallback;

  DeviceToDeviceInitiatorHelper();

  DeviceToDeviceInitiatorHelper(const DeviceToDeviceInitiatorHelper&) = delete;
  DeviceToDeviceInitiatorHelper& operator=(
      const DeviceToDeviceInitiatorHelper&) = delete;

  virtual ~DeviceToDeviceInitiatorHelper();

  // Creates the [Hello] message, which is the first message that is sent:
  // |session_public_key|: This session public key will be stored in plaintext
  //     (but signed) so the responder can parse it.
  // |persistent_symmetric_key|: The long-term symmetric key that is shared by
  //     the initiator and responder.
  // |secure_message_delegate|: Delegate for SecureMessage operations. This
  //     instance is not owned, and must live until after |callback| is invoked.
  // |callback|: Invoked upon operation completion with the serialized message
  //     or an empty string.
  void CreateHelloMessage(
      const std::string& session_public_key,
      const std::string& persistent_symmetric_key,
      multidevice::SecureMessageDelegate* secure_message_delegate,
      MessageCallback callback);

  // Validates that the [Responder Auth] message, received from the responder,
  // is properly signed and encrypted.
  // |responder_auth_message|: The bytes of the [Responder Auth] message to
  //     validate.
  // |persistent_responder_public_key|: The long-term public key possessed by
  //     the responder device.
  // |persistent_symmetric_key|: The long-term symmetric key that is shared by
  //     the initiator and responder.
  // |session_private_key|: The session private key is used in an Diffie-Helmann
  //     key exchange once the responder public key is extracted. The derived
  //     session symmetric key is used in the validation process.
  // |hello_message|: The initial [Hello] message that was sent, which is used
  //     in the signature calculation.
  // |secure_message_delegate|: Delegate for SecureMessage operations. This
  //     instance is not owned, and must live until after |callback| is invoked.
  // |callback|: Invoked upon operation completion with whether
  //     |responder_auth_message| is validated successfully.
  void ValidateResponderAuthMessage(
      const std::string& responder_auth_message,
      const std::string& persistent_responder_public_key,
      const std::string& persistent_symmetric_key,
      const std::string& session_private_key,
      const std::string& hello_message,
      multidevice::SecureMessageDelegate* secure_message_delegate,
      ValidateResponderAuthCallback callback);

  // Creates the [Initiator Auth] message, which allows the responder to
  // authenticate the initiator:
  // |session_keys|: The session symmetric keys.
  // |persistent_symmetric_key|: The long-term symmetric key that is shared by
  //     the initiator and responder.
  // |responder_auth_message|: The [Responder Auth] message sent previously to
  // the responder. These bytes are used in the signature calculation.
  // |secure_message_delegate|: Delegate for SecureMessage operations. This
  //     instance is not owned, and must live until after |callback| is invoked.
  // |callback|: Invoked upon operation completion with the serialized message
  //     or an empty string.
  void CreateInitiatorAuthMessage(
      const SessionKeys& session_keys,
      const std::string& persistent_symmetric_key,
      const std::string& responder_auth_message,
      multidevice::SecureMessageDelegate* secure_message_delegate,
      MessageCallback callback);

 private:
  // Helper struct containing all the context needed to validate the
  // [Responder Auth] message.
  struct ValidateResponderAuthMessageContext {
    ValidateResponderAuthMessageContext(
        const std::string& responder_auth_message,
        const std::string& persistent_responder_public_key,
        const std::string& persistent_symmetric_key,
        const std::string& session_private_key,
        const std::string& hello_message,
        multidevice::SecureMessageDelegate* secure_message_delegate);
    ValidateResponderAuthMessageContext(
        const ValidateResponderAuthMessageContext& other);
    ~ValidateResponderAuthMessageContext();

    std::string responder_auth_message;
    std::string persistent_responder_public_key;
    std::string persistent_symmetric_key;
    std::string session_private_key;
    std::string hello_message;
    raw_ptr<multidevice::SecureMessageDelegate> secure_message_delegate;
    std::string responder_session_public_key;
    std::string session_symmetric_key;
  };

  // Begins the [Responder Auth] validation flow by validating the header.
  void BeginResponderAuthValidation(ValidateResponderAuthMessageContext context,
                                    ValidateResponderAuthCallback callback);

  // Called after the session symmetric key is derived, so now we can unwrap the
  // outer message of [Responder Auth].
  void OnSessionSymmetricKeyDerived(ValidateResponderAuthMessageContext context,
                                    ValidateResponderAuthCallback callback,
                                    const std::string& session_symmetric_key);

  // Called after the outer-most layer of [Responder Auth] is unwrapped.
  void OnOuterMessageUnwrappedForResponderAuth(
      const ValidateResponderAuthMessageContext& context,
      ValidateResponderAuthCallback callback,
      bool verified,
      const std::string& payload,
      const securemessage::Header& header);

  // Called after the middle layer of [Responder Auth] is unwrapped.
  void OnMiddleMessageUnwrappedForResponderAuth(
      const ValidateResponderAuthMessageContext& context,
      ValidateResponderAuthCallback callback,
      bool verified,
      const std::string& payload,
      const securemessage::Header& header);

  // Called after inner message is created.
  void OnInnerMessageCreatedForInitiatorAuth(
      const SessionKeys& session_keys,
      multidevice::SecureMessageDelegate* secure_message_delegate,
      DeviceToDeviceInitiatorHelper::MessageCallback callback,
      const std::string& inner_message);

  // Callback for CreateInitiatorAuthMessage(), after the inner message is
  // created.
  void OnInnerMessageUnwrappedForResponderAuth(
      const ValidateResponderAuthMessageContext& context,
      ValidateResponderAuthCallback callback,
      bool verified,
      const std::string& payload,
      const securemessage::Header& header);

  base::WeakPtrFactory<DeviceToDeviceInitiatorHelper> weak_ptr_factory_{this};
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_INITIATOR_HELPER_H_
