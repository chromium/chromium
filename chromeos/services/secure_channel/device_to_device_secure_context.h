// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_SECURE_CONTEXT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_SECURE_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/secure_context.h"
#include "chromeos/services/secure_channel/session_keys.h"

namespace securemessage {
class Header;
}  // namespace securemessage

namespace chromeos {

namespace multidevice {
class SecureMessageDelegate;
}  // namespace multidevice

namespace secure_channel {

// SecureContext implementation for the DeviceToDevice protocol.
class DeviceToDeviceSecureContext : public SecureContext {
 public:
  DeviceToDeviceSecureContext(
      std::unique_ptr<multidevice::SecureMessageDelegate>
          secure_message_delegate,
      const SessionKeys& session_keys,
      const std::string& responder_auth_message_,
      ProtocolVersion protocol_version);

  ~DeviceToDeviceSecureContext() override;

  // SecureContext:
  void Decode(const std::string& encoded_message,
              const MessageCallback& callback) override;
  void Encode(const std::string& message,
              const MessageCallback& callback) override;
  ProtocolVersion GetProtocolVersion() const override;
  std::string GetChannelBindingData() const override;

 private:
  // Callback for unwrapping a secure message. |callback| will be invoked with
  // the decrypted payload if the message is unwrapped successfully; otherwise
  // it will be invoked with an empty string.
  void HandleUnwrapResult(
      const DeviceToDeviceSecureContext::MessageCallback& callback,
      bool verified,
      const std::string& payload,
      const securemessage::Header& header);

  // Delegate for handling the creation and unwrapping of SecureMessages.
  std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate_;

  // The symmetric key used for encryption.
  const std::string encryption_key_;

  // The symmetric key used for decryption.
  const std::string decryption_key_;

  // The [Responder Auth] message received from the remote device during
  // authentication.
  const std::string responder_auth_message_;

  // The protocol version supported by the remote device.
  const ProtocolVersion protocol_version_;

  // The last sequence number of the message sent.
  int last_encode_sequence_number_;

  // The last sequence number of the message received.
  int last_decode_sequence_number_;

  base::WeakPtrFactory<DeviceToDeviceSecureContext> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceToDeviceSecureContext);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_SECURE_CONTEXT_H_
