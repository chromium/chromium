// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_basic.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/legion_common.h"
#include "components/private_ai/proto/legion.pb.h"
#include "components/private_ai/secure_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

using ::testing::_;
using ::testing::Invoke;

class FakeSecureChannel : public SecureChannel {
 public:
  explicit FakeSecureChannel(ResponseCallback callback)
      : response_callback_(std::move(callback)) {
    CHECK(response_callback_);
  }

  ~FakeSecureChannel() override = default;

  bool Write(const Request& request) override {
    // Make sure that `request` is encoded `proto::LegionRequest` proto
    // struct.
    proto::LegionRequest request_proto;
    CHECK(request_proto.ParseFromArray(request.data(), request.size()));
    last_written_request_ = request_proto;

    return write_succeeds_;
  }

  // Test control methods:
  void set_write_succeeds(bool succeeds) { write_succeeds_ = succeeds; }

  const proto::LegionRequest& last_written_request() const {
    return last_written_request_;
  }
  void send_back_response(const proto::LegionResponse& response) {
    std::vector<uint8_t> response_bytes(response.ByteSizeLong());
    response.SerializeToArray(response_bytes.data(), response_bytes.size());

    CHECK(response_callback_);
    response_callback_.Run(std::move(response_bytes));
  }

  void send_back_error(ErrorCode error) {
    CHECK(response_callback_);
    response_callback_.Run(base::unexpected(error));
  }

 private:
  ResponseCallback response_callback_;
  proto::LegionRequest last_written_request_;
  bool write_succeeds_ = true;
};

class FakeSecureChannelFactory : public SecureChannel::Factory {
 public:
  using OnCreatedCallback = base::RepeatingCallback<void(FakeSecureChannel*)>;

  explicit FakeSecureChannelFactory(OnCreatedCallback on_created_callback)
      : on_created_callback_(std::move(on_created_callback)) {}

  ~FakeSecureChannelFactory() override = default;

  std::unique_ptr<SecureChannel> Create(
      SecureChannel::ResponseCallback callback) override {
    auto secure_channel =
        std::make_unique<FakeSecureChannel>(std::move(callback));
    on_created_callback_.Run(secure_channel.get());
    return secure_channel;
  }

 private:
  OnCreatedCallback on_created_callback_;
};

class ConnectionBasicTest : public testing::Test {
 public:
  void SetUp() override {
    connection_ = std::make_unique<ConnectionBasic>(
        std::make_unique<FakeSecureChannelFactory>(
            base::BindRepeating(&ConnectionBasicTest::on_secure_channel_created,
                                base::Unretained(this))),
        base::BindOnce(&ConnectionBasicTest::on_disconnect,
                       base::Unretained(this)));

    // `secure_channel_` should be created after creating a `connection_`.
    CHECK(secure_channel_);
  }

  void on_secure_channel_created(FakeSecureChannel* secure_channel) {
    secure_channel_ = secure_channel;
  }

  void on_disconnect() { on_disconnect_counter_++; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<Connection> connection_;
  raw_ptr<FakeSecureChannel> secure_channel_;

  int on_disconnect_counter_ = 0;
};

TEST_F(ConnectionBasicTest, Success) {
  base::test::TestFuture<base::expected<proto::LegionResponse, ErrorCode>>
      future;

  // Prepare request and send it.
  proto::LegionRequest request;
  request.set_feature_name(
      proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);
  connection_->Send(request, base::Seconds(1), future.GetCallback());

  // Verify that received request is expected.
  EXPECT_EQ(secure_channel_->last_written_request().request_id(), 1);
  EXPECT_EQ(secure_channel_->last_written_request().feature_name(),
            proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);

  // Prepare response and send it.
  proto::LegionResponse response;
  response.set_request_id(1);
  secure_channel_->send_back_response(response);

  // Verify that received response is expected.
  auto result = future.Get();
  EXPECT_TRUE(result.has_value());

  // Make sure that `on_disconnect` callback is NOT called.
  EXPECT_EQ(on_disconnect_counter_, 0);
}

// Tests that two requests are sent and the responses are received out of order,
// they are correctly matched to their callbacks.
TEST_F(ConnectionBasicTest, SuccessWithTwoRequests) {
  base::test::TestFuture<base::expected<proto::LegionResponse, ErrorCode>>
      future1;

  // Prepare request1 and send it.
  {
    proto::LegionRequest request1;
    request1.set_feature_name(
        proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);
    connection_->Send(request1, base::Seconds(1), future1.GetCallback());
  }

  // Verify that received request is expected.
  EXPECT_EQ(secure_channel_->last_written_request().request_id(), 1);
  EXPECT_EQ(secure_channel_->last_written_request().feature_name(),
            proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);

  base::test::TestFuture<base::expected<proto::LegionResponse, ErrorCode>>
      future2;

  // Prepare request2 and send it.
  {
    proto::LegionRequest request2;
    request2.set_feature_name(
        proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT);
    connection_->Send(request2, base::Seconds(1), future2.GetCallback());
  }

  // Verify that received request is expected.
  EXPECT_EQ(secure_channel_->last_written_request().request_id(), 2);
  EXPECT_EQ(secure_channel_->last_written_request().feature_name(),
            proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT);

  // Prepare response2 and send it first to make sure that the order
  // of responses does not matter.
  {
    proto::LegionResponse response2;
    response2.set_request_id(2);
    secure_channel_->send_back_response(response2);
  }

  // Prepare response1 and send it.
  {
    proto::LegionResponse response1;
    response1.set_request_id(1);
    secure_channel_->send_back_response(response1);
  }

  // Verify that received response are expected.
  {
    auto result1 = future1.Get();
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value().request_id(), 1);
  }

  {
    auto result2 = future2.Get();
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value().request_id(), 2);
  }

  // Make sure that `on_disconnect` callback is NOT called.
  EXPECT_EQ(on_disconnect_counter_, 0);
}

// Tests that if the secure channel returns an error, the request fails and
// the connection is disconnected.
TEST_F(ConnectionBasicTest, SecureChannelError) {
  base::test::TestFuture<base::expected<proto::LegionResponse, ErrorCode>>
      future;

  // Prepare request and send it.
  proto::LegionRequest request;
  request.set_feature_name(
      proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);
  connection_->Send(request, base::Seconds(1), future.GetCallback());

  // Verify that received request is expected.
  EXPECT_EQ(secure_channel_->last_written_request().request_id(), 1);
  EXPECT_EQ(secure_channel_->last_written_request().feature_name(),
            proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);

  // Send back an error.
  secure_channel_->send_back_error(ErrorCode::kError);

  // Verify that received response is expected.
  auto result = future.Get();
  EXPECT_FALSE(result.has_value());

  // Make sure that `on_disconnect` callback is called.
  EXPECT_EQ(on_disconnect_counter_, 1);
}

// Tests that if SecureChannel::Write returns false, the request fails and
// the connection is disconnected.
TEST_F(ConnectionBasicTest, SecureChannelWriteFails) {
  base::test::TestFuture<base::expected<proto::LegionResponse, ErrorCode>>
      future;

  // Prepare secure channel to fail Write operation.
  secure_channel_->set_write_succeeds(false);

  // Prepare request and send it.
  proto::LegionRequest request;
  request.set_feature_name(
      proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);
  connection_->Send(request, base::Seconds(1), future.GetCallback());

  // Verify that received request is expected.
  EXPECT_EQ(secure_channel_->last_written_request().request_id(), 1);
  EXPECT_EQ(secure_channel_->last_written_request().feature_name(),
            proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);

  // Verify that received response is expected.
  auto result = future.Get();
  EXPECT_FALSE(result.has_value());

  // Make sure that `on_disconnect` callback is called.
  EXPECT_EQ(on_disconnect_counter_, 1);
}

// Tests that when connection is disconnected, it does not send requests over
// the wire even if Send() function called with another request.
TEST_F(ConnectionBasicTest, SendOneMoreRequestAfterSecureChannelError) {
  base::test::TestFuture<base::expected<proto::LegionResponse, ErrorCode>>
      future;

  // Prepare request and send it.
  {
    proto::LegionRequest request;
    request.set_feature_name(
        proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);
    connection_->Send(request, base::Seconds(1), future.GetCallback());
  }

  // Verify that received request is expected.
  EXPECT_EQ(secure_channel_->last_written_request().request_id(), 1);
  EXPECT_EQ(secure_channel_->last_written_request().feature_name(),
            proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);

  // Send back an error.
  secure_channel_->send_back_error(ErrorCode::kError);

  // Verify that received response is expected.
  {
    auto result = future.Get();
    EXPECT_FALSE(result.has_value());
  }

  // Make sure that `on_disconnect` callback is called.
  EXPECT_EQ(on_disconnect_counter_, 1);

  // Prepare 2nd request and send it even though secure channel is not valid
  // anymore.

  base::test::TestFuture<base::expected<proto::LegionResponse, ErrorCode>>
      future2;

  {
    proto::LegionRequest request;
    request.set_feature_name(
        proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT);
    connection_->Send(request, base::Seconds(1), future2.GetCallback());
  }

  // Verify that 2nd request was not sent to the secure channel as it is
  // considered as invalid after returning an error.
  EXPECT_EQ(secure_channel_->last_written_request().request_id(), 1);
  EXPECT_EQ(secure_channel_->last_written_request().feature_name(),
            proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);

  // Verify that received response is expected.
  {
    auto result = future2.Get();
    EXPECT_FALSE(result.has_value());
  }

  // Make sure that `on_disconnect` callback is called only ONCE.
  EXPECT_EQ(on_disconnect_counter_, 1);
}

// Tests that when secure channel sends a response with unknown request_id,
// such response is ignored and does not affect the connection.
TEST_F(ConnectionBasicTest, SecureChannelUnknownRequestId) {
  // Prepare response with unknown request_id and send it.
  proto::LegionResponse response_unknown_request_id;
  response_unknown_request_id.set_request_id(777);
  secure_channel_->send_back_response(response_unknown_request_id);

  base::test::TestFuture<base::expected<proto::LegionResponse, ErrorCode>>
      future;

  // Prepare request and send it.
  proto::LegionRequest request;
  request.set_feature_name(
      proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);
  connection_->Send(request, base::Seconds(1), future.GetCallback());

  // Verify that received request is expected.
  EXPECT_EQ(secure_channel_->last_written_request().request_id(), 1);
  EXPECT_EQ(secure_channel_->last_written_request().feature_name(),
            proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);

  // Prepare response and send it.
  proto::LegionResponse response;
  response.set_request_id(1);
  secure_channel_->send_back_response(response);

  // Verify that received response is expected.
  auto result = future.Get();
  EXPECT_TRUE(result.has_value());

  // Make sure that `on_disconnect` callback is NOT called.
  EXPECT_EQ(on_disconnect_counter_, 0);
}

}  // namespace

}  // namespace private_ai
