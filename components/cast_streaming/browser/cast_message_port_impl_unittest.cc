// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/cast_message_port_impl.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "components/cast_streaming/common/message_serialization.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {

namespace {
const char kSenderId[] = "senderId";
}  // namespace

class CastMessagePortImplTest : public testing::Test,
                                public openscreen::cast::MessagePort::Client {
 public:
  CastMessagePortImplTest() = default;
  ~CastMessagePortImplTest() override = default;

  CastMessagePortImplTest(const CastMessagePortImplTest&) = delete;
  CastMessagePortImplTest& operator=(const CastMessagePortImplTest&) = delete;

  void SetUp() override {
    std::unique_ptr<cast_api_bindings::MessagePort> receiver;
    cast_api_bindings::CreatePlatformMessagePortPair(&sender_message_port_,
                                                     &receiver);

    sender_message_port_->SetReceiver(&sender_message_port_receiver_);
    receiver_message_port_ = std::make_unique<CastMessagePortImpl>(
        std::move(receiver),
        base::BindOnce(&CastMessagePortImplTest::OnCastChannelClosed,
                       base::Unretained(this)));
    receiver_message_port_->SetClient(*this);
  }

 protected:
  struct CastMessage {
    std::string sender_id;
    std::string message_namespace;
    std::string message;
  };

  void RunUntilMessageCountIsAtLeast(size_t message_count) {
    while (receiver_messages_.size() < message_count) {
      base::RunLoop run_loop;
      receiver_message_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  void RunUntilError() {
    base::RunLoop run_loop;
    error_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void RunUntilCastChannelClosed() {
    base::RunLoop run_loop;
    cast_channel_closed_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnCastChannelClosed() {
    if (cast_channel_closed_closure_) {
      std::move(cast_channel_closed_closure_).Run();
    } else {
      ADD_FAILURE() << "Cast Streaming Session MessagePort disconnected";
    }
  }

  // openscreen::cast::MessagePort::Client implementation.
  void OnMessage(const std::string& source_sender_id,
                 const std::string& message_namespace,
                 const std::string& message) override {
    receiver_messages_.push_back(
        {source_sender_id, message_namespace, message});
    if (receiver_message_closure_) {
      std::move(receiver_message_closure_).Run();
    }
  }
  void OnError(const openscreen::Error& error) override {
    latest_error_ = error;
    if (error_closure_) {
      std::move(error_closure_).Run();
    }
  }
  const std::string& source_id() override { return source_id_; }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  openscreen::Error latest_error_ = openscreen::Error::None();
  std::vector<CastMessage> receiver_messages_;
  base::OnceClosure receiver_message_closure_;
  base::OnceClosure error_closure_;
  base::OnceClosure cast_channel_closed_closure_;

  std::unique_ptr<CastMessagePortImpl> receiver_message_port_;
  std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port_;
  cast_api_bindings::TestMessagePortReceiver sender_message_port_receiver_;
  std::string source_id_ = kSenderId;
};

// Tests basic connection between the sender and receiver message port is
// working.
TEST_F(CastMessagePortImplTest, BasicConnection) {
  std::string sender_id;
  std::string message_namespace;
  std::string message;
  const std::string test_message = "testMessage";

  // Check the initial connect message is properly sent.
  sender_message_port_receiver_.RunUntilMessageCountEqual(1u);
  ASSERT_EQ(sender_message_port_receiver_.buffer().size(), 1u);
  ASSERT_TRUE(
      DeserializeCastMessage(sender_message_port_receiver_.buffer().at(0).first,
                             &sender_id, &message_namespace, &message));
  EXPECT_EQ(sender_id, kValueSystemSenderId);
  EXPECT_EQ(message_namespace, kSystemNamespace);
  EXPECT_EQ(message, kInitialConnectMessage);

  // Check the the connection from the sender to the receiver is working.
  sender_message_port_->PostMessage(
      SerializeCastMessage(kSenderId, kMirroringNamespace, test_message));
  RunUntilMessageCountIsAtLeast(1u);
  ASSERT_EQ(receiver_messages_.size(), 1u);
  EXPECT_EQ(receiver_messages_.at(0).sender_id, kSenderId);
  EXPECT_EQ(receiver_messages_.at(0).message_namespace, kMirroringNamespace);
  EXPECT_EQ(receiver_messages_.at(0).message, test_message);

  // Check the connection from the receiver to the sender is working.
  receiver_message_port_->PostMessage(kSenderId, kMirroringNamespace,
                                      test_message);
  sender_message_port_receiver_.RunUntilMessageCountEqual(2u);
  ASSERT_EQ(sender_message_port_receiver_.buffer().size(), 2u);
  ASSERT_TRUE(
      DeserializeCastMessage(sender_message_port_receiver_.buffer().at(1).first,
                             &sender_id, &message_namespace, &message));
  EXPECT_EQ(sender_id, kSenderId);
  EXPECT_EQ(message_namespace, kMirroringNamespace);
  EXPECT_EQ(message, test_message);
}

// Tests the "not supported" message is properly received for the inject
// message.
TEST_F(CastMessagePortImplTest, InjectMessage) {
  const int kRequestId = 42;
  base::Value::Dict inject_value;
  inject_value.Set(kKeyType, kValueWrapped);
  inject_value.Set(kKeyRequestId, kRequestId);
  std::string inject_message;
  ASSERT_TRUE(base::JSONWriter::Write(inject_value, &inject_message));

  sender_message_port_->PostMessage(
      SerializeCastMessage(kSenderId, kInjectNamespace, inject_message));
  sender_message_port_receiver_.RunUntilMessageCountEqual(2u);
  ASSERT_EQ(sender_message_port_receiver_.buffer().size(), 2u);

  std::string sender_id;
  std::string message_namespace;
  std::string message;
  ASSERT_TRUE(
      DeserializeCastMessage(sender_message_port_receiver_.buffer().at(1).first,
                             &sender_id, &message_namespace, &message));
  EXPECT_EQ(sender_id, kSenderId);
  EXPECT_EQ(message_namespace, kInjectNamespace);

  std::optional<base::Value> return_value = base::JSONReader::Read(message);
  ASSERT_TRUE(return_value);
  ASSERT_TRUE(return_value->is_dict());

  const std::string* type_value = return_value->GetDict().FindString(kKeyType);
  ASSERT_TRUE(type_value);
  EXPECT_EQ(*type_value, kValueError);

  std::optional<int> request_id_value =
      return_value->GetDict().FindInt(kKeyRequestId);
  ASSERT_TRUE(request_id_value);
  EXPECT_EQ(request_id_value.value(), kRequestId);

  const std::string* data_value = return_value->GetDict().FindString(kKeyData);
  ASSERT_TRUE(data_value);
  EXPECT_EQ(*data_value, kValueInjectNotSupportedError);

  const std::string* code_value = return_value->GetDict().FindString(kKeyCode);
  ASSERT_TRUE(code_value);
  EXPECT_EQ(*code_value, kValueWrappedError);
}

// Tests sending a bad message properly reports an error to Open Screen without
// crashing.
TEST_F(CastMessagePortImplTest, BadMessage) {
  const std::string kBadMessage = "42";
  sender_message_port_->PostMessage(kBadMessage);
  RunUntilError();
  EXPECT_EQ(latest_error_,
            openscreen::Error(openscreen::Error::Code::kCastV2InvalidMessage));
}

// Tests closing the sender-end of the Cast Channel properly runs the closure.
TEST_F(CastMessagePortImplTest, CastChannelClosed) {
  sender_message_port_.reset();
  RunUntilCastChannelClosed();
}

// Tests the media status namespace is properly handled.
TEST_F(CastMessagePortImplTest, MediaStatus) {
  const int kRequestId = 42;
  base::Value::Dict media_value;
  media_value.Set(kKeyType, kValueMediaGetStatus);
  media_value.Set(kKeyRequestId, kRequestId);
  std::string media_message;
  ASSERT_TRUE(base::JSONWriter::Write(media_value, &media_message));

  sender_message_port_->PostMessage(
      SerializeCastMessage(kSenderId, kMediaNamespace, media_message));
  sender_message_port_receiver_.RunUntilMessageCountEqual(2u);
  ASSERT_EQ(sender_message_port_receiver_.buffer().size(), 2u);

  std::string sender_id;
  std::string message_namespace;
  std::string message;
  ASSERT_TRUE(
      DeserializeCastMessage(sender_message_port_receiver_.buffer().at(1).first,
                             &sender_id, &message_namespace, &message));
  EXPECT_EQ(sender_id, kSenderId);
  EXPECT_EQ(message_namespace, kMediaNamespace);

  std::optional<base::Value> return_value = base::JSONReader::Read(message);
  ASSERT_TRUE(return_value);
  ASSERT_TRUE(return_value->is_dict());

  const std::string* type_value = return_value->GetDict().FindString(kKeyType);
  ASSERT_TRUE(type_value);
  EXPECT_EQ(*type_value, kValueMediaStatus);

  std::optional<int> request_id_value =
      return_value->GetDict().FindInt(kKeyRequestId);
  ASSERT_TRUE(request_id_value);
  EXPECT_EQ(request_id_value.value(), kRequestId);

  const base::Value::List* status_value =
      return_value->GetDict().FindList(kKeyStatus);
  ASSERT_TRUE(status_value);
  EXPECT_EQ(status_value->size(), 1u);
}

// Checks sending invalid media messages results in no response.
TEST_F(CastMessagePortImplTest, InvalidMediaMessages) {
  const int kRequestId = 42;

  {
    // Send an invalid message value.
    sender_message_port_->PostMessage(
        SerializeCastMessage(kSenderId, kMediaNamespace, "not a json"));
  }

  {
    // Send a non-dictionary value.
    std::string media_message;
    ASSERT_TRUE(
        base::JSONWriter::Write(base::Value("string value"), &media_message));
    sender_message_port_->PostMessage(
        SerializeCastMessage(kSenderId, kMediaNamespace, media_message));
  }

  {
    // Send a message with no type.
    base::Value::Dict media_value;
    media_value.Set(kKeyRequestId, kRequestId);
    std::string media_message;
    ASSERT_TRUE(base::JSONWriter::Write(media_value, &media_message));
    sender_message_port_->PostMessage(
        SerializeCastMessage(kSenderId, kMediaNamespace, media_message));
  }

  {
    // Send a PLAY message. This is not incorrect but should be ignored.
    base::Value::Dict media_value;
    media_value.Set(kKeyType, kValueMediaPlay);
    media_value.Set(kKeyRequestId, kRequestId);
    std::string media_message;
    ASSERT_TRUE(base::JSONWriter::Write(media_value, &media_message));
    sender_message_port_->PostMessage(
        SerializeCastMessage(kSenderId, kMediaNamespace, media_message));
  }

  {
    // Send a PAUSE message. This is not incorrect but should be ignored.
    base::Value::Dict media_value;
    media_value.Set(kKeyType, kValueMediaPause);
    media_value.Set(kKeyRequestId, kRequestId);
    std::string media_message;
    ASSERT_TRUE(base::JSONWriter::Write(media_value, &media_message));
    sender_message_port_->PostMessage(
        SerializeCastMessage(kSenderId, kMediaNamespace, media_message));
  }

  {
    // Send a message with an invalid type.
    base::Value::Dict media_value;
    media_value.Set(kKeyType, "INVALID_TYPE");
    media_value.Set(kKeyRequestId, kRequestId);
    std::string media_message;
    ASSERT_TRUE(base::JSONWriter::Write(media_value, &media_message));
    sender_message_port_->PostMessage(
        SerializeCastMessage(kSenderId, kMediaNamespace, media_message));
  }

  {
    // Send a GET_STATUS message with no request ID.
    base::Value::Dict media_value;
    media_value.Set(kKeyType, kValueMediaGetStatus);
    std::string media_message;
    ASSERT_TRUE(base::JSONWriter::Write(media_value, &media_message));
    sender_message_port_->PostMessage(
        SerializeCastMessage(kSenderId, kMediaNamespace, media_message));
  }

  {
    // Send a message with a non-integer request ID.
    base::Value::Dict media_value;
    media_value.Set(kKeyType, kValueMediaGetStatus);
    media_value.Set(kKeyRequestId, "not an integer");
    std::string media_message;
    ASSERT_TRUE(base::JSONWriter::Write(media_value, &media_message));
    sender_message_port_->PostMessage(
        SerializeCastMessage(kSenderId, kMediaNamespace, media_message));
  }

  // Process all the things. There should be a single message (the original
  // system message), since none of the other messages sent will trigger a
  // response.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(sender_message_port_receiver_.buffer().size(), 1u);
}

}  // namespace cast_streaming
