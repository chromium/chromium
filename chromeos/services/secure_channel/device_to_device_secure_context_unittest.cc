// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/device_to_device_secure_context.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/secure_channel/session_keys.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/securemessage/proto/securemessage.pb.h"

namespace chromeos {

namespace secure_channel {

namespace {

const char kSymmetricKey[] = "symmetric key";
const char kResponderAuthMessage[] = "responder_auth_message";
const SecureContext::ProtocolVersion kProtocolVersion =
    SecureContext::PROTOCOL_VERSION_THREE_ONE;

// Callback saving |result| to |result_out|.
void SaveResult(std::string* result_out, const std::string& result) {
  *result_out = result;
}

// The responder's secure context will have the encoding / decoding keys
// inverted.
class InvertedSessionKeys : public SessionKeys {
 public:
  explicit InvertedSessionKeys(const std::string& session_symmetric_key)
      : SessionKeys(session_symmetric_key) {}

  InvertedSessionKeys() : SessionKeys() {}

  InvertedSessionKeys(const InvertedSessionKeys& other) : SessionKeys(other) {}

  std::string initiator_encode_key() const override {
    return SessionKeys::responder_encode_key();
  }
  std::string responder_encode_key() const override {
    return SessionKeys::initiator_encode_key();
  }
};

}  // namespace

class SecureChannelDeviceToDeviceSecureContextTest : public testing::Test {
 protected:
  SecureChannelDeviceToDeviceSecureContextTest()
      : secure_context_(
            std::make_unique<multidevice::FakeSecureMessageDelegate>(),
            SessionKeys(kSymmetricKey),
            kResponderAuthMessage,
            kProtocolVersion) {}

  DeviceToDeviceSecureContext secure_context_;
};

TEST_F(SecureChannelDeviceToDeviceSecureContextTest, GetProperties) {
  EXPECT_EQ(kResponderAuthMessage, secure_context_.GetChannelBindingData());
  EXPECT_EQ(kProtocolVersion, secure_context_.GetProtocolVersion());
}

TEST_F(SecureChannelDeviceToDeviceSecureContextTest, CheckEncodedHeader) {
  std::string message = "encrypt this message";
  std::string encoded_message;
  secure_context_.Encode(message,
                         base::BindOnce(&SaveResult, &encoded_message));

  securemessage::SecureMessage secure_message;
  ASSERT_TRUE(secure_message.ParseFromString(encoded_message));
  securemessage::HeaderAndBody header_and_body;
  ASSERT_TRUE(
      header_and_body.ParseFromString(secure_message.header_and_body()));

  cryptauth::GcmMetadata gcm_metadata;
  ASSERT_TRUE(
      gcm_metadata.ParseFromString(header_and_body.header().public_metadata()));
  EXPECT_EQ(1, gcm_metadata.version());
  EXPECT_EQ(cryptauth::DEVICE_TO_DEVICE_MESSAGE, gcm_metadata.type());
}

TEST_F(SecureChannelDeviceToDeviceSecureContextTest, DecodeInvalidMessage) {
  std::string encoded_message = "invalidly encoded message";
  std::string decoded_message = "not empty";
  secure_context_.Decode(encoded_message,
                         base::BindOnce(&SaveResult, &decoded_message));
  EXPECT_TRUE(decoded_message.empty());
}

TEST_F(SecureChannelDeviceToDeviceSecureContextTest, EncodeAndDecode) {
  // Initialize second secure channel with the same parameters as the first.
  InvertedSessionKeys inverted_session_keys(kSymmetricKey);
  DeviceToDeviceSecureContext secure_context2(
      std::make_unique<multidevice::FakeSecureMessageDelegate>(),
      inverted_session_keys, kResponderAuthMessage, kProtocolVersion);
  std::string message = "encrypt this message";

  SessionKeys session_keys(kSymmetricKey);
  EXPECT_EQ(session_keys.initiator_encode_key(),
            inverted_session_keys.responder_encode_key());
  EXPECT_EQ(session_keys.responder_encode_key(),
            inverted_session_keys.initiator_encode_key());

  // Pass some messages between the two secure contexts.
  for (int i = 0; i < 3; ++i) {
    std::string encoded_message;
    secure_context_.Encode(message,
                           base::BindOnce(&SaveResult, &encoded_message));
    EXPECT_NE(message, encoded_message);

    std::string decoded_message;
    secure_context2.Decode(encoded_message,
                           base::BindOnce(&SaveResult, &decoded_message));
    EXPECT_EQ(message, decoded_message);
  }
}

TEST_F(SecureChannelDeviceToDeviceSecureContextTest,
       DecodeInvalidSequenceNumber) {
  // Initialize second secure channel with the same parameters as the first.
  DeviceToDeviceSecureContext secure_context2(
      std::make_unique<multidevice::FakeSecureMessageDelegate>(),
      InvertedSessionKeys(kSymmetricKey), kResponderAuthMessage,
      kProtocolVersion);

  // Send a few messages over the first secure context.
  std::string message = "encrypt this message";
  std::string encoded1;
  for (int i = 0; i < 3; ++i) {
    secure_context_.Encode(message, base::BindOnce(&SaveResult, &encoded1));
  }

  // Second secure channel should not decode the message with an invalid
  // sequence number.
  std::string decoded_message = "not empty";
  secure_context_.Decode(encoded1,
                         base::BindOnce(&SaveResult, &decoded_message));
  EXPECT_TRUE(decoded_message.empty());
}

}  // namespace secure_channel

}  // namespace chromeos
