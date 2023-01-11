// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/device_to_device_secure_context.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/secure_channel/session_keys.h"
#include "third_party/securemessage/proto/securemessage.pb.h"

namespace ash::secure_channel {

namespace {

// The version to put in the cryptauth::GcmMetadata field.
const int kGcmMetadataVersion = 1;

// The sequence number of the last message sent during authentication. These
// messages are sent and received before the SecureContext is created.
const int kAuthenticationEncodeSequenceNumber = 1;

// The sequence number of the last message received during authentication. These
// messages are sent and received before the SecureContext is created.
const int kAuthenticationDecodeSequenceNumber = 1;

}  // namespace

DeviceToDeviceSecureContext::DeviceToDeviceSecureContext(
    std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate,
    const SessionKeys& session_keys,
    const std::string& responder_auth_message,
    ProtocolVersion protocol_version)
    : secure_message_delegate_(std::move(secure_message_delegate)),
      encryption_key_(session_keys.initiator_encode_key()),
      decryption_key_(session_keys.responder_encode_key()),
      responder_auth_message_(responder_auth_message),
      protocol_version_(protocol_version),
      last_encode_sequence_number_(kAuthenticationEncodeSequenceNumber),
      last_decode_sequence_number_(kAuthenticationDecodeSequenceNumber) {}

DeviceToDeviceSecureContext::~DeviceToDeviceSecureContext() {}

void DeviceToDeviceSecureContext::DecodeAndDequeue(
    const std::string& encoded_message,
    DecodeMessageCallback callback) {
  multidevice::SecureMessageDelegate::UnwrapOptions unwrap_options;
  unwrap_options.encryption_scheme = securemessage::AES_256_CBC;
  unwrap_options.signature_scheme = securemessage::HMAC_SHA256;

  secure_message_delegate_->UnwrapSecureMessage(
      encoded_message, decryption_key_, unwrap_options,
      base::BindOnce(&DeviceToDeviceSecureContext::HandleUnwrapResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceToDeviceSecureContext::Encode(const std::string& message,
                                         EncodeMessageCallback callback) {
  // Create a cryptauth::GcmMetadata field to put in the header.
  cryptauth::GcmMetadata gcm_metadata;
  gcm_metadata.set_type(cryptauth::DEVICE_TO_DEVICE_MESSAGE);
  gcm_metadata.set_version(kGcmMetadataVersion);

  // Wrap |message| inside a DeviceToDeviceMessage proto.
  securegcm::DeviceToDeviceMessage device_to_device_message;
  device_to_device_message.set_sequence_number(++last_encode_sequence_number_);
  device_to_device_message.set_message(message);

  multidevice::SecureMessageDelegate::CreateOptions create_options;
  create_options.encryption_scheme = securemessage::AES_256_CBC;
  create_options.signature_scheme = securemessage::HMAC_SHA256;
  gcm_metadata.SerializeToString(&create_options.public_metadata);

  secure_message_delegate_->CreateSecureMessage(
      device_to_device_message.SerializeAsString(), encryption_key_,
      create_options, std::move(callback));
}

std::string DeviceToDeviceSecureContext::GetChannelBindingData() const {
  return responder_auth_message_;
}

SecureContext::ProtocolVersion DeviceToDeviceSecureContext::GetProtocolVersion()
    const {
  return protocol_version_;
}

void DeviceToDeviceSecureContext::ProcessIncomingMessageQueue(
    DeviceToDeviceSecureContext::DecodeMessageCallback callback) {
  while (!incoming_message_queue_.empty() &&
         incoming_message_queue_.top().sequence_number() ==
             last_decode_sequence_number_ + 1) {
    callback.Run(incoming_message_queue_.top().message());
    PA_LOG(INFO) << "Queued incomming message sequence_number="
                 << incoming_message_queue_.top().sequence_number()
                 << " is processed.";
    last_decode_sequence_number_++;
    incoming_message_queue_.pop();
  }
}

void DeviceToDeviceSecureContext::HandleUnwrapResult(
    DeviceToDeviceSecureContext::DecodeMessageCallback callback,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header) {
  // The payload should contain a DeviceToDeviceMessage proto.
  securegcm::DeviceToDeviceMessage device_to_device_message;
  if (!verified || !device_to_device_message.ParseFromString(payload)) {
    PA_LOG(ERROR) << "Failed to unwrap secure message.";
    std::move(callback).Run(std::string());
    return;
  }

  // Check that the sequence number matches the expected sequence number.
  if (device_to_device_message.sequence_number() !=
      last_decode_sequence_number_ + 1) {
    PA_LOG(WARNING) << "Expected sequence_number="
                    << last_decode_sequence_number_ + 1 << ", but got "
                    << device_to_device_message.sequence_number();
    if (device_to_device_message.sequence_number() >
        last_decode_sequence_number_ + 1) {
      PA_LOG(INFO) << "Queue incoming message sequence_number="
                   << device_to_device_message.sequence_number();
      incoming_message_queue_.push(device_to_device_message);
    } else {
      PA_LOG(ERROR)
          << "Drop incoming message due to incorrect sequence number.";
    }
    return;
  }

  // Validate the cryptauth::GcmMetadata proto in the header.
  cryptauth::GcmMetadata gcm_metadata;
  if (!gcm_metadata.ParseFromString(header.public_metadata()) ||
      gcm_metadata.type() != cryptauth::DEVICE_TO_DEVICE_MESSAGE ||
      gcm_metadata.version() != kGcmMetadataVersion) {
    PA_LOG(ERROR) << "Failed to validate cryptauth::GcmMetadata.";
    std::move(callback).Run(std::string());
    return;
  }

  last_decode_sequence_number_++;
  callback.Run(device_to_device_message.message());
  ProcessIncomingMessageQueue(callback);
}

}  // namespace ash::secure_channel
