// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/device_to_device_secure_context.h"

#include <list>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/ash/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/secure_channel/session_keys.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/securemessage/proto/securemessage.pb.h"

namespace ash::secure_channel {

namespace {

const char kSymmetricKey[] = "symmetric key";
const char kResponderAuthMessage[] = "responder_auth_message";
const SecureContext::ProtocolVersion kProtocolVersion =
    SecureContext::PROTOCOL_VERSION_THREE_ONE;

// Callback saving |result| to |result_out|.
void SaveResultEncode(std::string* result_out, const std::string& result) {
  *result_out = result;
}

// Callback saving |result| to |result_out|.
void SaveResultDecode(std::list<std::string>* result_out_list,
                      const std::string& result) {
  result_out_list->push_back(std::move(result));
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
                         base::BindOnce(&SaveResultEncode, &encoded_message));

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
  std::list<std::string> decoded_messages;
  secure_context_.DecodeAndDequeue(
      encoded_message,
      base::BindRepeating(&SaveResultDecode, &decoded_messages));
  EXPECT_TRUE(decoded_messages.front().empty());
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
                           base::BindOnce(&SaveResultEncode, &encoded_message));
    EXPECT_NE(message, encoded_message);

    std::list<std::string> decoded_messages;
    secure_context2.DecodeAndDequeue(
        encoded_message,
        base::BindRepeating(&SaveResultDecode, &decoded_messages));
    EXPECT_EQ(message, decoded_messages.front());
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
    secure_context_.Encode(message,
                           base::BindOnce(&SaveResultEncode, &encoded1));
  }

  // Second secure channel should not decode the message with an invalid
  // sequence number.
  std::list<std::string> decoded_messages;
  secure_context2.DecodeAndDequeue(
      encoded1, base::BindRepeating(&SaveResultDecode, &decoded_messages));
  EXPECT_TRUE(decoded_messages.empty());
}

TEST_F(SecureChannelDeviceToDeviceSecureContextTest, DecodeOutOfOrderMessages) {
  // Initialize second secure channel with the same parameters as the first.
  DeviceToDeviceSecureContext secure_context2(
      std::make_unique<multidevice::FakeSecureMessageDelegate>(),
      InvertedSessionKeys(kSymmetricKey), kResponderAuthMessage,
      kProtocolVersion);

  // Send a few messages over the first secure context.
  std::string message1 = "encrypt this message 1";
  std::string encoded1;
  std::string message2 = "encrypt this message 2";
  std::string encoded2;
  secure_context_.Encode(message1,
                         base::BindOnce(&SaveResultEncode, &encoded1));
  secure_context_.Encode(message2,
                         base::BindOnce(&SaveResultEncode, &encoded2));

  // Second secure channel should queue out of order message and trigger when
  // old message is received.
  std::list<std::string> decoded_messages;
  secure_context2.DecodeAndDequeue(
      encoded2, base::BindRepeating(&SaveResultDecode, &decoded_messages));
  secure_context2.DecodeAndDequeue(
      encoded1, base::BindRepeating(&SaveResultDecode, &decoded_messages));
  EXPECT_EQ(message1, decoded_messages.front());
  decoded_messages.pop_front();
  EXPECT_EQ(message2, decoded_messages.front());
}

TEST_F(SecureChannelDeviceToDeviceSecureContextTest,
       DecodeWithoutFirstMissingMessage) {
  // Initialize second secure channel with the same parameters as the first.
  DeviceToDeviceSecureContext secure_context2(
      std::make_unique<multidevice::FakeSecureMessageDelegate>(),
      InvertedSessionKeys(kSymmetricKey), kResponderAuthMessage,
      kProtocolVersion);

  // Send a few messages over the first secure context.
  std::string message1 = "encrypt this message 1";
  std::string encoded1;
  std::string message2 = "encrypt this message 2";
  std::string encoded2;
  std::string message3 = "encrypt this message 3";
  std::string encoded3;
  secure_context_.Encode(message1,
                         base::BindOnce(&SaveResultEncode, &encoded1));
  secure_context_.Encode(message2,
                         base::BindOnce(&SaveResultEncode, &encoded2));
  secure_context_.Encode(message3,
                         base::BindOnce(&SaveResultEncode, &encoded3));

  // Second secure channel should not decode the message without first missing
  // message is received
  std::list<std::string> decoded_messages;
  secure_context2.DecodeAndDequeue(
      encoded2, base::BindRepeating(&SaveResultDecode, &decoded_messages));
  secure_context2.DecodeAndDequeue(
      encoded3, base::BindRepeating(&SaveResultDecode, &decoded_messages));
  EXPECT_TRUE(decoded_messages.empty());
  // Second secure channel should decode once first missing message is received
  secure_context2.DecodeAndDequeue(
      encoded1, base::BindRepeating(&SaveResultDecode, &decoded_messages));
  EXPECT_EQ(message1, decoded_messages.front());
  decoded_messages.pop_front();
  EXPECT_EQ(message2, decoded_messages.front());
  decoded_messages.pop_front();
  EXPECT_EQ(message3, decoded_messages.front());
}

TEST_F(SecureChannelDeviceToDeviceSecureContextTest,
       DecodeMultipleOutOfOrderMessages) {
  // Initialize second secure channel with the same parameters as the first.
  DeviceToDeviceSecureContext secure_context2(
      std::make_unique<multidevice::FakeSecureMessageDelegate>(),
      InvertedSessionKeys(kSymmetricKey), kResponderAuthMessage,
      kProtocolVersion);

  // Send a few messages over the first secure context.
  std::string message1 = "encrypt this message 1";
  std::string encoded1;
  std::string message2 = "encrypt this message 2";
  std::string encoded2;
  std::string message3 = "encrypt this message 3";
  std::string encoded3;
  std::string message4 = "encrypt this message 4";
  std::string encoded4;

  secure_context_.Encode(message1,
                         base::BindOnce(&SaveResultEncode, &encoded1));
  secure_context_.Encode(message2,
                         base::BindOnce(&SaveResultEncode, &encoded2));
  secure_context_.Encode(message3,
                         base::BindOnce(&SaveResultEncode, &encoded3));
  secure_context_.Encode(message4,
                         base::BindOnce(&SaveResultEncode, &encoded4));

  // Second secure channel should not decode the messages with multiple out of
  // order messages
  std::list<std::string> decoded_messages;
  secure_context2.DecodeAndDequeue(
      encoded2, base::BindRepeating(&SaveResultDecode, &decoded_messages));
  secure_context2.DecodeAndDequeue(
      encoded4, base::BindRepeating(&SaveResultDecode, &decoded_messages));
  EXPECT_TRUE(decoded_messages.empty());
  // Second secure channel should decode the messages in order, the others
  // should remain in queue.
  secure_context2.DecodeAndDequeue(
      encoded1, base::BindRepeating(&SaveResultDecode, &decoded_messages));
  EXPECT_TRUE(decoded_messages.size() == 2);
  EXPECT_EQ(message1, decoded_messages.front());
  decoded_messages.pop_front();
  EXPECT_EQ(message2, decoded_messages.front());
  // Second secure channel should decode the rest of message when the remained
  // messages are in order.
  decoded_messages.clear();
  secure_context2.DecodeAndDequeue(
      encoded3, base::BindRepeating(&SaveResultDecode, &decoded_messages));
  EXPECT_TRUE(decoded_messages.size() == 2);
  EXPECT_EQ(message3, decoded_messages.front());
  decoded_messages.pop_front();
  EXPECT_EQ(message4, decoded_messages.front());
}

}  // namespace ash::secure_channel
