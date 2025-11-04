// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/client.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

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

namespace {

// Mock implementation of the SecureChannel interface.
class MockSecureChannelClient : public SecureChannel {
 public:
  MockSecureChannelClient() = default;
  ~MockSecureChannelClient() override = default;

  MOCK_METHOD(
      void,
      Write,
      (Request request,
       base::OnceCallback<void(base::expected<Response, ErrorCode>)> callback),
      (override));
};

}  // namespace

class ClientTest : public ::testing::Test {
 public:
  ClientTest() = default;
  ~ClientTest() override = default;

  void SetUp() override {
    auto mock_secure_channel =
        std::make_unique<testing::StrictMock<MockSecureChannelClient>>();
    mock_secure_channel_ = mock_secure_channel.get();
    client_ = Client(std::move(mock_secure_channel),
                     proto::FeatureName::FEATURE_NAME_UNSPECIFIED);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::optional<Client> client_;
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
  Response response_data(serialized_response.begin(),
                         serialized_response.end());

  EXPECT_CALL(*mock_secure_channel_, Write(_, _))
      .WillOnce(RunOnceCallback<1>(base::ok(response_data)));

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
class ClientSendTextRequestErrorTest
    : public ClientTest,
      public ::testing::WithParamInterface<proto::LegionResponse> {};

TEST_P(ClientSendTextRequestErrorTest, SendTextRequestMalformedResponse) {
  proto::LegionResponse legion_response = GetParam();

  std::string serialized_response;
  legion_response.SerializeToString(&serialized_response);
  Response response_data(serialized_response.begin(),
                         serialized_response.end());

  EXPECT_CALL(*mock_secure_channel_, Write(_, _))
      .WillOnce(RunOnceCallback<1>(base::ok(response_data)));

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest("some text", future.GetCallback());

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kNoContent);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ClientSendTextRequestErrorTest,
                         ::testing::Values(
                             // Empty response.
                             [] {
                               proto::LegionResponse response;
                               response.mutable_generate_content_response();
                               return response;
                             }(),
                             // Response with no content parts.
                             [] {
                               proto::LegionResponse response;
                               auto* generate_content_response =
                                   response.mutable_generate_content_response();
                               generate_content_response->add_candidates();
                               return response;
                             }()));

// Test fixture for error conditions in SendGenerateContentRequest where the
// server response is malformed.
class ClientSendGenerateContentRequestErrorTest
    : public ClientTest,
      public ::testing::WithParamInterface<std::pair<Response, ErrorCode>> {};

TEST_P(ClientSendGenerateContentRequestErrorTest,
       SendGenerateContentRequestMalformedResponse) {
  const auto& param = GetParam();
  Response response_data = param.first;

  EXPECT_CALL(*mock_secure_channel_, Write(_, _))
      .WillOnce(RunOnceCallback<1>(base::ok(response_data)));

  base::test::TestFuture<
      base::expected<proto::GenerateContentResponse, ErrorCode>>
      future;
  client_->SendGenerateContentRequest(proto::GenerateContentRequest(),
                                      future.GetCallback());

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), param.second);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ClientSendGenerateContentRequestErrorTest,
    ::testing::Values(
        // Invalid response that cannot be parsed.
        std::make_pair(Response{0x01, 0x02, 0x03},
                       ErrorCode::kResponseParseError),
        // Response missing GenerateContentResponse.
        std::make_pair(
            [] {
              proto::LegionResponse legion_response;
              std::string serialized_response;
              legion_response.SerializeToString(&serialized_response);
              return Response(serialized_response.begin(),
                              serialized_response.end());
            }(),
            ErrorCode::kNoResponse)));

}  // namespace legion
