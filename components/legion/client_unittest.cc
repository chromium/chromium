// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/client.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/legion/proto/legion.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace legion {

using ::testing::_;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::WithArgs;

namespace {

// Mock implementation of the SecureChannel interface.
class MockSecureChannelClient : public SecureChannel {
 public:
  MockSecureChannelClient() = default;
  ~MockSecureChannelClient() override = default;

  MOCK_METHOD(void,
              SetResponseCallback,
              (ResponseCallback callback),
              (override));
  MOCK_METHOD(bool, Write, (const Request& request), (override));
};

class FakeSecureChannelFactory {
 public:
  FakeSecureChannelFactory() = default;
  ~FakeSecureChannelFactory() = default;

  std::unique_ptr<SecureChannel> Create() {
    auto channel =
        std::make_unique<testing::StrictMock<MockSecureChannelClient>>();
    EXPECT_CALL(*channel, SetResponseCallback(_))
        .WillOnce(testing::SaveArg<0>(&response_callback_));
    secure_channel_ = channel.get();
    return channel;
  }

  raw_ptr<MockSecureChannelClient> secure_channel_ = nullptr;
  SecureChannel::ResponseCallback response_callback_;
};

struct ResponseErrorTestParam {
  Client::BinaryEncodedProtoResponse response_data;
  ErrorCode expected_error;
  bool mismatch_request_id = false;
};

void SetUpMockWrite(MockSecureChannelClient* mock_secure_channel,
                    SecureChannel::ResponseCallback& response_callback,
                    const Client::BinaryEncodedProtoResponse& response_template,
                    bool mismatch_request_id = false) {
  EXPECT_CALL(*mock_secure_channel, Write(_))
      .WillOnce([=, &response_callback](const Request& request_payload) {
        proto::LegionRequest request;
        EXPECT_TRUE(request.ParseFromArray(request_payload.data(),
                                           request_payload.size()));

        proto::LegionResponse response;
        Client::BinaryEncodedProtoResponse response_data;
        if (response.ParseFromArray(response_template.data(),
                                    response_template.size())) {
          if (mismatch_request_id) {
            response.set_request_id(request.request_id() + 1);
          } else {
            response.set_request_id(request.request_id());
          }
          std::string serialized;
          response.SerializeToString(&serialized);
          response_data.assign(serialized.begin(), serialized.end());
        } else {
          response_data = response_template;
        }

        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(response_callback,
                                      base::ok(std::move(response_data))));
        return true;
      });
}

}  // namespace

class ClientTest : public ::testing::Test {
 public:
  ClientTest() = default;
  ~ClientTest() override = default;

  void SetUp() override {
    client_ = base::WrapUnique(new Client(base::BindRepeating(
        &FakeSecureChannelFactory::Create, base::Unretained(&factory_))));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<Client> client_;
  FakeSecureChannelFactory factory_;
};

// Test the successful request flow.
TEST_F(ClientTest, SendTextRequestSuccess) {
  const std::string kExpectedResponseText = "response text";

  proto::LegionResponse legion_response;
  auto* generate_content_response =
      legion_response.mutable_generate_content_response();
  auto* candidate = generate_content_response->add_candidates();
  auto* content = candidate->mutable_content();
  content->set_role("model");
  auto* part = content->add_parts();
  part->set_text(kExpectedResponseText);

  std::string serialized_response;
  legion_response.SerializeToString(&serialized_response);
  Client::BinaryEncodedProtoResponse response_data(serialized_response.begin(),
                                                   serialized_response.end());

  SetUpMockWrite(factory_.secure_channel_, factory_.response_callback_,
                 response_data);

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback());

  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), kExpectedResponseText);
}

// Test that SendRequest fails if SecureChannel::Write fails.
TEST_F(ClientTest, SendTextRequestWriteFails) {
  EXPECT_CALL(*factory_.secure_channel_, Write(_))
      .WillOnce(testing::Return(false));

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback());

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kError);
}

// Test that a response with an unknown request_id is ignored.
TEST_F(ClientTest, IgnoresResponseWithUnknownRequestId) {
  const std::string kExpectedResponseText = "response text";

  proto::LegionResponse legion_response;
  auto* generate_content_response =
      legion_response.mutable_generate_content_response();
  auto* candidate = generate_content_response->add_candidates();
  auto* content = candidate->mutable_content();
  content->set_role("model");
  auto* part = content->add_parts();
  part->set_text(kExpectedResponseText);

  std::string serialized_response;
  legion_response.SerializeToString(&serialized_response);
  Client::BinaryEncodedProtoResponse response_data(serialized_response.begin(),
                                                   serialized_response.end());

  // Set up mock to respond with a mismatched request ID.
  SetUpMockWrite(factory_.secure_channel_, factory_.response_callback_,
                 response_data,
                 /*mismatch_request_id=*/true);

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback());

  // Run all pending tasks. The response callback in the client should have been
  // called, but it should have done nothing.
  task_environment_.RunUntilIdle();

  // The future should not have been completed.
  EXPECT_FALSE(future.IsReady());
}

// Test that the secure channel is recreated after a permanent failure.
TEST_F(ClientTest, SecureChannelRecreation) {
  auto* first_channel = factory_.secure_channel_.get();
  EXPECT_CALL(*factory_.secure_channel_, Write(_))
      .WillOnce(testing::Return(true));

  // Send a request that will fail.
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback());

  // Simulate a network error from the secure channel. This should trigger a
  // channel recreation.
  factory_.response_callback_.Run(base::unexpected(ErrorCode::kNetworkError));

  // The request should have failed with the same error.
  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kNetworkError);

  // A new channel should have been created.
  auto second_channel = factory_.secure_channel_;
  EXPECT_NE(first_channel, second_channel.get());

  // A subsequent request should succeed on the new channel.
  const std::string kExpectedResponseText = "response text";

  proto::LegionResponse legion_response;
  auto* generate_content_response =
      legion_response.mutable_generate_content_response();
  auto* candidate = generate_content_response->add_candidates();
  auto* content = candidate->mutable_content();
  content->set_role("model");
  auto* part = content->add_parts();
  part->set_text(kExpectedResponseText);

  std::string serialized_response;
  legion_response.SerializeToString(&serialized_response);
  Client::BinaryEncodedProtoResponse response_data(serialized_response.begin(),
                                                   serialized_response.end());

  SetUpMockWrite(second_channel, factory_.response_callback_, response_data);

  base::test::TestFuture<base::expected<std::string, ErrorCode>> second_future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some other text", second_future.GetCallback());

  const auto& second_result = second_future.Get();
  ASSERT_TRUE(second_result.has_value());
  EXPECT_EQ(second_result.value(), kExpectedResponseText);
}

// Test that a request times out correctly.
TEST_F(ClientTest, SendTextRequestTimeout) {
  // Mock the secure channel to never respond.
  EXPECT_CALL(*factory_.secure_channel_, Write(_))
      .WillOnce(testing::Return(true));

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback(), base::Seconds(10));

  // The request is sent but no response is received yet.
  ASSERT_FALSE(future.IsReady());

  // Advance time to trigger the timeout callback.
  task_environment_.FastForwardBy(base::Seconds(10));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kTimeout);
}

// Test that a response received after a timeout is ignored.
TEST_F(ClientTest, SendTextRequestResponseAfterTimeout) {
  // Mock the secure channel to not invoke the response callback.
  EXPECT_CALL(*factory_.secure_channel_, Write(_))
      .WillOnce(testing::Return(true));

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback(), base::Seconds(10));

  // The request is sent but no response is received yet.
  ASSERT_FALSE(future.IsReady());

  // Advance time to trigger the timeout callback.
  task_environment_.FastForwardBy(base::Seconds(10));

  // The future should now be ready with a timeout error.
  ASSERT_TRUE(future.IsReady());
  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kTimeout);

  // Now, simulate the response arriving late. This should not cause a crash or
  // change the result.
  proto::LegionResponse legion_response;
  legion_response.set_request_id(1);
  legion_response.mutable_generate_content_response()
      ->add_candidates()
      ->mutable_content()
      ->add_parts()
      ->set_text("late response");
  std::string serialized_response;
  legion_response.SerializeToString(&serialized_response);
  Client::BinaryEncodedProtoResponse response_data(serialized_response.begin(),
                                                   serialized_response.end());
  factory_.response_callback_.Run(base::ok(response_data));

  // To ensure the task runner has a chance to run the callback (which should be
  // a no-op), we can run until idle.
  task_environment_.RunUntilIdle();
}

// Test fixture for error conditions in SendTextRequest where the
// SecureChannel returns an error.
class ClientSendTextRequestSecureChannelErrorTest
    : public ClientTest,
      public ::testing::WithParamInterface<ErrorCode> {};

TEST_P(ClientSendTextRequestSecureChannelErrorTest, SendTextRequestError) {
  ErrorCode error_code = GetParam();
  EXPECT_CALL(*factory_.secure_channel_, Write(_))
      .WillOnce([&](const Request& request) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(factory_.response_callback_,
                                      base::unexpected(error_code)));
        return true;
      });

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback());

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), error_code);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ClientSendTextRequestSecureChannelErrorTest,
                         ::testing::Values(ErrorCode::kNetworkError,
                                           ErrorCode::kError));

// Test fixture for error conditions in SendTextRequest where the server
// response is malformed.
class ClientSendTextRequestResponseErrorTest
    : public ClientTest,
      public ::testing::WithParamInterface<ResponseErrorTestParam> {};

TEST_P(ClientSendTextRequestResponseErrorTest, SendTextRequestError) {
  const auto& param = GetParam();

  SetUpMockWrite(factory_.secure_channel_, factory_.response_callback_,
                 param.response_data, param.mismatch_request_id);

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback());

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), param.expected_error);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ClientSendTextRequestResponseErrorTest,
    ::testing::Values(
        // Empty response.
        ResponseErrorTestParam{
            .response_data =
                [] {
                  proto::LegionResponse response;
                  response.mutable_generate_content_response();
                  std::string serialized;
                  response.SerializeToString(&serialized);
                  return Client::BinaryEncodedProtoResponse(serialized.begin(),
                                                            serialized.end());
                }(),
            .expected_error = ErrorCode::kNoContent},
        // Response with no content parts.
        ResponseErrorTestParam{
            .response_data =
                [] {
                  proto::LegionResponse response;
                  auto* gcr = response.mutable_generate_content_response();
                  gcr->add_candidates();
                  std::string serialized;
                  response.SerializeToString(&serialized);
                  return Client::BinaryEncodedProtoResponse(serialized.begin(),
                                                            serialized.end());
                }(),
            .expected_error = ErrorCode::kNoContent}));

// Test fixture for error conditions in SendGenerateContentRequest where the
// server response is malformed.
class ClientSendGenerateContentRequestErrorTest
    : public ClientTest,
      public ::testing::WithParamInterface<ResponseErrorTestParam> {};

TEST_P(ClientSendGenerateContentRequestErrorTest,
       SendGenerateContentRequestMalformedResponse) {
  const auto& param = GetParam();

  SetUpMockWrite(factory_.secure_channel_, factory_.response_callback_,
                 param.response_data, param.mismatch_request_id);

  base::test::TestFuture<
      base::expected<proto::GenerateContentResponse, ErrorCode>>
      future;
  client_->SendGenerateContentRequest(
      proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
      proto::GenerateContentRequest(), future.GetCallback());

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), param.expected_error);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ClientSendGenerateContentRequestErrorTest,
    ::testing::Values(
        // Invalid response that cannot be parsed.
        ResponseErrorTestParam{
            .response_data = {0x01, 0x02, 0x03},
            .expected_error = ErrorCode::kResponseParseError},
        // Response missing GenerateContentResponse.
        ResponseErrorTestParam{
            .response_data =
                [] {
                  proto::LegionResponse legion_response;
                  std::string serialized_response;
                  legion_response.SerializeToString(&serialized_response);
                  return Client::BinaryEncodedProtoResponse(
                      serialized_response.begin(), serialized_response.end());
                }(),
            .expected_error = ErrorCode::kNoResponse}));

}  // namespace legion
