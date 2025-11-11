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
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/legion/proto/legion.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace legion {

using base::test::RunOnceCallback;
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

  MOCK_METHOD(
      void,
      Write,
      (Client::BinaryEncodedProtoRequest request,
       base::OnceCallback<
           void(base::expected<Client::BinaryEncodedProtoResponse, ErrorCode>)>
           callback),
      (override));
};

struct ResponseErrorTestParam {
  Client::BinaryEncodedProtoResponse response_data;
  ErrorCode expected_error;
  bool mismatch_request_id = false;
};

void SetUpMockWrite(MockSecureChannelClient* mock_secure_channel,
                    const Client::BinaryEncodedProtoResponse& response_template,
                    bool mismatch_request_id = false) {
  EXPECT_CALL(*mock_secure_channel, Write(_, _))
      .WillOnce(WithArgs<0, 1>([=](const auto& request_payload, auto callback) {
        proto::LegionRequest request;
        ASSERT_TRUE(request.ParseFromArray(request_payload.data(),
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
          // If template is not a valid proto, send it as is.
          response_data = response_template;
        }

        std::move(callback).Run(base::ok(response_data));
      }));
}

}  // namespace

class ClientTest : public ::testing::Test {
 public:
  ClientTest() = default;
  ~ClientTest() override = default;

  void SetUp() override {
    auto mock_secure_channel =
        std::make_unique<testing::StrictMock<MockSecureChannelClient>>();
    mock_secure_channel_ = mock_secure_channel.get();
    client_ = base::WrapUnique(
        new Client(std::move(mock_secure_channel),
                   proto::FeatureName::FEATURE_NAME_UNSPECIFIED));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<Client> client_;
  raw_ptr<MockSecureChannelClient> mock_secure_channel_;  // Owned by client_
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

  SetUpMockWrite(mock_secure_channel_, response_data);

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest("some text", future.GetCallback());

  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), kExpectedResponseText);
}

// Test fixture for error conditions in SendTextRequest where the
// SecureChannel returns an error.
class ClientSendTextRequestSecureChannelErrorTest
    : public ClientTest,
      public ::testing::WithParamInterface<ErrorCode> {};

TEST_P(ClientSendTextRequestSecureChannelErrorTest, SendTextRequestError) {
  ErrorCode error_code = GetParam();
  EXPECT_CALL(*mock_secure_channel_, Write(_, _))
      .WillOnce(RunOnceCallback<1>(base::unexpected(error_code)));

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest("some text", future.GetCallback());

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

  SetUpMockWrite(mock_secure_channel_, param.response_data,
                 param.mismatch_request_id);

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest("some text", future.GetCallback());

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), param.expected_error);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ClientSendTextRequestResponseErrorTest,
    ::testing::Values(
        // Request ID mismatch.
        ResponseErrorTestParam{
            .response_data =
                [] {
                  proto::LegionResponse response;
                  response.set_request_id(999);
                  auto* gcr = response.mutable_generate_content_response();
                  auto* candidate = gcr->add_candidates();
                  candidate->mutable_content()->add_parts()->set_text("text");
                  std::string serialized;
                  response.SerializeToString(&serialized);
                  return Client::BinaryEncodedProtoResponse(serialized.begin(),
                                                            serialized.end());
                }(),
            .expected_error = ErrorCode::kError,
            .mismatch_request_id = true},
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

  SetUpMockWrite(mock_secure_channel_, param.response_data,
                 param.mismatch_request_id);

  base::test::TestFuture<
      base::expected<proto::GenerateContentResponse, ErrorCode>>
      future;
  client_->SendGenerateContentRequest(proto::GenerateContentRequest(),
                                      future.GetCallback());

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), param.expected_error);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ClientSendGenerateContentRequestErrorTest,
    ::testing::Values(
        // Request ID mismatch.
        ResponseErrorTestParam{
            .response_data =
                [] {
                  proto::LegionResponse response;
                  response.set_request_id(999);
                  response.mutable_generate_content_response();
                  std::string serialized;
                  response.SerializeToString(&serialized);
                  return Client::BinaryEncodedProtoResponse(serialized.begin(),
                                                            serialized.end());
                }(),
            .expected_error = ErrorCode::kError,
            .mismatch_request_id = true},
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
