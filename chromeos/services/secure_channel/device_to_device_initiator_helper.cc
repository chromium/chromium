// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/device_to_device_initiator_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/secure_message_delegate.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"

namespace chromeos {

namespace secure_channel {

namespace {

// TODO(tengs): Due to a bug with the ChromeOS secure message daemon, we cannot
// create SecureMessages with empty payloads. To workaround this bug, this value
// is put into the payload if it would otherwise be empty.
// See crbug.com/512894.
const char kPayloadFiller[] = "\xae";

// The version to put in the cryptauth::GcmMetadata field.
const int kGcmMetadataVersion = 1;

// The D2D protocol version.
const int kD2DProtocolVersion = 1;

}  // namespace

DeviceToDeviceInitiatorHelper::DeviceToDeviceInitiatorHelper() {}

DeviceToDeviceInitiatorHelper::~DeviceToDeviceInitiatorHelper() {}

void DeviceToDeviceInitiatorHelper::CreateHelloMessage(
    const std::string& session_public_key,
    const std::string& persistent_symmetric_key,
    multidevice::SecureMessageDelegate* secure_message_delegate,
    const MessageCallback& callback) {
  // Decode public key into the |initator_hello| proto.
  securemessage::InitiatorHello initiator_hello;
  if (!initiator_hello.mutable_public_dh_key()->ParseFromString(
          session_public_key)) {
    PA_LOG(ERROR) << "Unable to parse user's public key";
    callback.Run(std::string());
    return;
  }
  initiator_hello.set_protocol_version(kD2DProtocolVersion);

  // The [Hello] message has the structure:
  // {
  //   header: <session_public_key>,
  //           Sig(<session_public_key>, persistent_symmetric_key)
  //   payload: ""
  // }
  multidevice::SecureMessageDelegate::CreateOptions create_options;
  create_options.encryption_scheme = securemessage::NONE;
  create_options.signature_scheme = securemessage::HMAC_SHA256;
  initiator_hello.SerializeToString(&create_options.public_metadata);
  secure_message_delegate->CreateSecureMessage(
      kPayloadFiller, persistent_symmetric_key, create_options, callback);
}

void DeviceToDeviceInitiatorHelper::ValidateResponderAuthMessage(
    const std::string& responder_auth_message,
    const std::string& persistent_responder_public_key,
    const std::string& persistent_symmetric_key,
    const std::string& session_private_key,
    const std::string& hello_message,
    multidevice::SecureMessageDelegate* secure_message_delegate,
    const ValidateResponderAuthCallback& callback) {
  // The [Responder Auth] message has the structure:
  // {
  //   header: <responder_public_key>,
  //           Sig(<responder_public_key> + payload1,
  //               session_symmetric_key),
  //   payload1: Enc({
  //     header: Sig(payload2 + <hello_message>, persistent_symmetric_key),
  //     payload2: {
  //       sequence_number: 1,
  //       body: Enc({
  //         header: Sig(payload3 + <hello_message>,
  //                     persistent_responder_public_key),
  //         payload3: ""
  //       }, persistent_symmetric_key)
  //     }
  //   }, session_symmetric_key),
  // }
  ValidateResponderAuthMessageContext context(
      responder_auth_message, persistent_responder_public_key,
      persistent_symmetric_key, session_private_key, hello_message,
      secure_message_delegate, callback);
  BeginResponderAuthValidation(context);
}

void DeviceToDeviceInitiatorHelper::CreateInitiatorAuthMessage(
    const SessionKeys& session_keys,
    const std::string& persistent_symmetric_key,
    const std::string& responder_auth_message,
    multidevice::SecureMessageDelegate* secure_message_delegate,
    const MessageCallback& callback) {
  // The [Initiator Auth] message has the structure:
  // {
  //   header: Sig(payload1, session_symmetric_key)
  //   payload1: Enc({
  //     sequence_number: 2,
  //     body: {
  //       header: Sig(payload2 + responder_auth_message,
  //                   persistent_symmetric_key)
  //       payload2: ""
  //     }
  //   }, session_symmetric_key)
  // }
  multidevice::SecureMessageDelegate::CreateOptions create_options;
  create_options.encryption_scheme = securemessage::AES_256_CBC;
  create_options.signature_scheme = securemessage::HMAC_SHA256;
  create_options.associated_data = responder_auth_message;
  secure_message_delegate->CreateSecureMessage(
      kPayloadFiller, persistent_symmetric_key, create_options,
      base::Bind(
          &DeviceToDeviceInitiatorHelper::OnInnerMessageCreatedForInitiatorAuth,
          weak_ptr_factory_.GetWeakPtr(), session_keys, secure_message_delegate,
          callback));
}

DeviceToDeviceInitiatorHelper::ValidateResponderAuthMessageContext ::
    ValidateResponderAuthMessageContext(
        const std::string& responder_auth_message,
        const std::string& persistent_responder_public_key,
        const std::string& persistent_symmetric_key,
        const std::string& session_private_key,
        const std::string& hello_message,
        multidevice::SecureMessageDelegate* secure_message_delegate,
        const ValidateResponderAuthCallback& callback)
    : responder_auth_message(responder_auth_message),
      persistent_responder_public_key(persistent_responder_public_key),
      persistent_symmetric_key(persistent_symmetric_key),
      session_private_key(session_private_key),
      hello_message(hello_message),
      secure_message_delegate(secure_message_delegate),
      callback(callback) {}

DeviceToDeviceInitiatorHelper::ValidateResponderAuthMessageContext ::
    ValidateResponderAuthMessageContext(
        const ValidateResponderAuthMessageContext& other)
    : responder_auth_message(other.responder_auth_message),
      persistent_responder_public_key(other.persistent_responder_public_key),
      persistent_symmetric_key(other.persistent_symmetric_key),
      session_private_key(other.session_private_key),
      hello_message(other.hello_message),
      secure_message_delegate(other.secure_message_delegate),
      callback(other.callback),
      responder_session_public_key(other.responder_session_public_key),
      session_symmetric_key(other.session_symmetric_key) {}

DeviceToDeviceInitiatorHelper::ValidateResponderAuthMessageContext ::
    ~ValidateResponderAuthMessageContext() {}

void DeviceToDeviceInitiatorHelper::OnInnerMessageCreatedForInitiatorAuth(
    const SessionKeys& session_keys,
    multidevice::SecureMessageDelegate* secure_message_delegate,
    const DeviceToDeviceInitiatorHelper::MessageCallback& callback,
    const std::string& inner_message) {
  if (inner_message.empty()) {
    PA_LOG(VERBOSE) << "Failed to create inner message for [Initiator Auth].";
    callback.Run(std::string());
    return;
  }

  cryptauth::GcmMetadata gcm_metadata;
  gcm_metadata.set_type(cryptauth::DEVICE_TO_DEVICE_MESSAGE);
  gcm_metadata.set_version(kGcmMetadataVersion);

  // Store the inner message inside a DeviceToDeviceMessage proto.
  securemessage::DeviceToDeviceMessage device_to_device_message;
  device_to_device_message.set_message(inner_message);
  device_to_device_message.set_sequence_number(1);

  // Create and return the outer message, which wraps the inner message.
  multidevice::SecureMessageDelegate::CreateOptions create_options;
  create_options.encryption_scheme = securemessage::AES_256_CBC;
  create_options.signature_scheme = securemessage::HMAC_SHA256;
  gcm_metadata.SerializeToString(&create_options.public_metadata);
  secure_message_delegate->CreateSecureMessage(
      device_to_device_message.SerializeAsString(),
      session_keys.initiator_encode_key(), create_options, callback);
}

void DeviceToDeviceInitiatorHelper::BeginResponderAuthValidation(
    ValidateResponderAuthMessageContext context) {
  // Parse the encrypted SecureMessage so we can get plaintext data from the
  // header. Note that the payload will be encrypted.
  securemessage::SecureMessage encrypted_message;
  securemessage::HeaderAndBody header_and_body;
  if (!encrypted_message.ParseFromString(context.responder_auth_message) ||
      !header_and_body.ParseFromString(encrypted_message.header_and_body())) {
    PA_LOG(WARNING) << "Failed to parse [Responder Hello] message";
    context.callback.Run(false, SessionKeys());
    return;
  }

  // Check that header public_metadata contains the correct metadata fields.
  securemessage::Header header = header_and_body.header();
  cryptauth::GcmMetadata gcm_metadata;
  if (!gcm_metadata.ParseFromString(header.public_metadata()) ||
      gcm_metadata.type() !=
          cryptauth::DEVICE_TO_DEVICE_RESPONDER_HELLO_PAYLOAD ||
      gcm_metadata.version() != kGcmMetadataVersion) {
    PA_LOG(WARNING) << "Failed to validate cryptauth::GcmMetadata in "
                    << "[Responder Auth] header.";
    context.callback.Run(false, SessionKeys());
    return;
  }

  // Extract responder session public key from |decryption_key_id| field.
  securemessage::ResponderHello responder_hello;
  if (!responder_hello.ParseFromString(header.decryption_key_id()) ||
      !responder_hello.public_dh_key().SerializeToString(
          &context.responder_session_public_key)) {
    PA_LOG(VERBOSE) << "Failed to extract responder session public key in "
                    << "[Responder Auth] header.";
    context.callback.Run(false, SessionKeys());
    return;
  }

  // Perform a Diffie-Helmann key exchange to get the session symmetric key.
  context.secure_message_delegate->DeriveKey(
      context.session_private_key, context.responder_session_public_key,
      base::Bind(&DeviceToDeviceInitiatorHelper::OnSessionSymmetricKeyDerived,
                 weak_ptr_factory_.GetWeakPtr(), context));
}

void DeviceToDeviceInitiatorHelper::OnSessionSymmetricKeyDerived(
    ValidateResponderAuthMessageContext context,
    const std::string& session_symmetric_key) {
  context.session_symmetric_key = session_symmetric_key;

  // Unwrap the outer message, using symmetric key encryption and signature.
  multidevice::SecureMessageDelegate::UnwrapOptions unwrap_options;
  unwrap_options.encryption_scheme = securemessage::AES_256_CBC;
  unwrap_options.signature_scheme = securemessage::HMAC_SHA256;
  context.secure_message_delegate->UnwrapSecureMessage(
      context.responder_auth_message,
      SessionKeys(session_symmetric_key).responder_encode_key(), unwrap_options,
      base::Bind(&DeviceToDeviceInitiatorHelper::
                     OnOuterMessageUnwrappedForResponderAuth,
                 weak_ptr_factory_.GetWeakPtr(), context));
}

void DeviceToDeviceInitiatorHelper::OnOuterMessageUnwrappedForResponderAuth(
    const ValidateResponderAuthMessageContext& context,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header) {
  if (!verified) {
    PA_LOG(VERBOSE) << "Failed to unwrap outer [Responder Auth] message.";
    context.callback.Run(false, SessionKeys());
    return;
  }

  // Parse the decrypted payload.
  securemessage::DeviceToDeviceMessage device_to_device_message;
  if (!device_to_device_message.ParseFromString(payload) ||
      device_to_device_message.sequence_number() != 1) {
    PA_LOG(VERBOSE) << "Failed to validate DeviceToDeviceMessage payload.";
    context.callback.Run(false, SessionKeys());
    return;
  }

  // Unwrap the middle level SecureMessage, using symmetric key encryption and
  // signature.
  multidevice::SecureMessageDelegate::UnwrapOptions unwrap_options;
  unwrap_options.encryption_scheme = securemessage::AES_256_CBC;
  unwrap_options.signature_scheme = securemessage::HMAC_SHA256;
  unwrap_options.associated_data = context.hello_message;
  context.secure_message_delegate->UnwrapSecureMessage(
      device_to_device_message.message(), context.persistent_symmetric_key,
      unwrap_options,
      base::Bind(&DeviceToDeviceInitiatorHelper::
                     OnMiddleMessageUnwrappedForResponderAuth,
                 weak_ptr_factory_.GetWeakPtr(), context));
}

void DeviceToDeviceInitiatorHelper::OnMiddleMessageUnwrappedForResponderAuth(
    const ValidateResponderAuthMessageContext& context,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header) {
  if (!verified) {
    PA_LOG(VERBOSE) << "Failed to unwrap middle [Responder Auth] message.";
    context.callback.Run(false, SessionKeys());
    return;
  }

  // Unwrap the inner-most SecureMessage, using no encryption and an asymmetric
  // key signature.
  multidevice::SecureMessageDelegate::UnwrapOptions unwrap_options;
  unwrap_options.encryption_scheme = securemessage::NONE;
  unwrap_options.signature_scheme = securemessage::ECDSA_P256_SHA256;
  unwrap_options.associated_data = context.hello_message;
  context.secure_message_delegate->UnwrapSecureMessage(
      payload, context.persistent_responder_public_key, unwrap_options,
      base::Bind(&DeviceToDeviceInitiatorHelper::
                     OnInnerMessageUnwrappedForResponderAuth,
                 weak_ptr_factory_.GetWeakPtr(), context));
}

// Called after the inner-most layer of [Responder Auth] is unwrapped.
void DeviceToDeviceInitiatorHelper::OnInnerMessageUnwrappedForResponderAuth(
    const ValidateResponderAuthMessageContext& context,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header) {
  if (!verified)
    PA_LOG(VERBOSE) << "Failed to unwrap inner [Responder Auth] message.";

  // Note: The GMS Core implementation does not properly set the metadata
  // version, so we only check that the type is UNLOCK_KEY_SIGNED_CHALLENGE.
  cryptauth::GcmMetadata gcm_metadata;
  if (!gcm_metadata.ParseFromString(header.public_metadata()) ||
      gcm_metadata.type() != cryptauth::UNLOCK_KEY_SIGNED_CHALLENGE) {
    PA_LOG(WARNING)
        << "Failed to validate cryptauth::GcmMetadata in inner-most "
        << "[Responder Auth] message.";
    context.callback.Run(false, SessionKeys());
    return;
  }

  context.callback.Run(verified, SessionKeys(context.session_symmetric_key));
}

}  // namespace secure_channel

}  // namespace chromeos
