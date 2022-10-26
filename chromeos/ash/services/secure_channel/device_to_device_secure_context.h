// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_SECURE_CONTEXT_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_SECURE_CONTEXT_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/secure_channel/secure_context.h"
#include "third_party/ukey2/proto/device_to_device_messages.pb.h"

namespace securemessage {
class Header;
}

namespace ash {

namespace multidevice {
class SecureMessageDelegate;
}

namespace secure_channel {

class SessionKeys;

struct MessageComparator {
  // Prioritize messages with the lowest sequence number
  bool operator()(securegcm::DeviceToDeviceMessage a,
                  securegcm::DeviceToDeviceMessage b) {
    return a.sequence_number() > b.sequence_number();
  }
};

// SecureContext implementation for the DeviceToDevice protocol.
class DeviceToDeviceSecureContext : public SecureContext {
 public:
  DeviceToDeviceSecureContext(
      std::unique_ptr<multidevice::SecureMessageDelegate>
          secure_message_delegate,
      const SessionKeys& session_keys,
      const std::string& responder_auth_message_,
      ProtocolVersion protocol_version);

  DeviceToDeviceSecureContext(const DeviceToDeviceSecureContext&) = delete;
  DeviceToDeviceSecureContext& operator=(const DeviceToDeviceSecureContext&) =
      delete;

  ~DeviceToDeviceSecureContext() override;

  // SecureContext:
  void DecodeAndDequeue(const std::string& encoded_message,
                        DecodeMessageCallback callback) override;
  void Encode(const std::string& message,
              EncodeMessageCallback callback) override;
  ProtocolVersion GetProtocolVersion() const override;
  std::string GetChannelBindingData() const override;

 private:
  // Callback for unwrapping a secure message. |callback| will be invoked with
  // the decrypted payload if the message is unwrapped successfully; otherwise
  // it will be invoked with an empty string.
  void HandleUnwrapResult(
      DeviceToDeviceSecureContext::DecodeMessageCallback callback,
      bool verified,
      const std::string& payload,
      const securemessage::Header& header);

  // Process the queued SecureMessages.
  void ProcessIncomingMessageQueue(
      DeviceToDeviceSecureContext::DecodeMessageCallback callback);

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

  // The priority queue for caching out of order DeviceToDeviceMessage
  std::priority_queue<securegcm::DeviceToDeviceMessage,
                      std::vector<securegcm::DeviceToDeviceMessage>,
                      MessageComparator>
      incoming_message_queue_;

  base::WeakPtrFactory<DeviceToDeviceSecureContext> weak_ptr_factory_{this};
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_TO_DEVICE_SECURE_CONTEXT_H_
