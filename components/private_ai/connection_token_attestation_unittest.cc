// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_token_attestation.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/testing/fake_connection.h"
#include "components/private_ai/testing/fake_token_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

class ConnectionTokenAttestationTest : public testing::Test {
 public:
  ConnectionTokenAttestationTest() = default;
  ~ConnectionTokenAttestationTest() override = default;

  void CreateConnectionAttestation() {
    auto fake_connection = std::make_unique<FakeConnection>(base::DoNothing());
    fake_connection_ = fake_connection.get();
    connection_attestation_ = std::make_unique<ConnectionTokenAttestation>(
        std::move(fake_connection), &token_manager_, &logger_,
        base::BindOnce(&ConnectionTokenAttestationTest::OnDisconnect,
                       base::Unretained(this)));
  }

  void OnDisconnect(ErrorCode error_code) {
    on_disconnect_counter_++;
    connection_attestation_->OnDestroy(error_code);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  FakeTokenManager token_manager_;
  PrivateAiLogger logger_;

  std::unique_ptr<ConnectionTokenAttestation> connection_attestation_;
  raw_ptr<FakeConnection> fake_connection_;

  int on_disconnect_counter_ = 0;
};

TEST_F(ConnectionTokenAttestationTest, Success) {
  CreateConnectionAttestation();

  // No requests should be sent yet, waiting for token.
  EXPECT_EQ(fake_connection_->pending_requests().size(), 0u);

  // Buffer a request.
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;
  proto::PrivateAiRequest request;
  request.set_request_id(123);
  connection_attestation_->Send(std::move(request), base::Seconds(1),
                                future.GetCallback());

  // Still no requests sent.
  EXPECT_EQ(fake_connection_->pending_requests().size(), 0u);

  // Provide the token.
  token_manager_.RunPendingCallbacks();

  // Attestation request should be sent.
  ASSERT_EQ(fake_connection_->pending_requests().size(), 1u);
  {
    const auto& pending_request =
        fake_connection_->pending_requests()[0].request;
    ASSERT_TRUE(pending_request.has_anonymous_token_request());
    EXPECT_EQ(pending_request.anonymous_token_request().anonymous_token(),
              FakeTokenManager::kFakeToken);
  }

  // Respond to the attestation request.
  proto::PrivateAiResponse attestation_response;
  std::move(fake_connection_->pending_requests()[0].callback)
      .Run(std::move(attestation_response));

  // Now the buffered request should be sent.
  ASSERT_EQ(fake_connection_->pending_requests().size(), 2u);
  EXPECT_EQ(fake_connection_->pending_requests()[1].request.request_id(), 123);

  // Respond to the original request.
  proto::PrivateAiResponse response;
  response.set_request_id(123);
  std::move(fake_connection_->pending_requests()[1].callback)
      .Run(std::move(response));

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().request_id(), 123);

  EXPECT_EQ(on_disconnect_counter_, 0);
}

TEST_F(ConnectionTokenAttestationTest, NoToken) {
  token_manager_.SetReturnToken(false);
  CreateConnectionAttestation();

  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;
  connection_attestation_->Send(proto::PrivateAiRequest(), base::Seconds(1),
                                future.GetCallback());

  // No requests sent.
  EXPECT_EQ(fake_connection_->pending_requests().size(), 0u);

  // Fail to provide the token.
  token_manager_.RunPendingCallbacks();

  // Pending request should fail.
  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kClientAttestationFailed);

  // No requests were ever sent to the inner connection.
  EXPECT_EQ(fake_connection_->pending_requests().size(), 0u);

  EXPECT_EQ(on_disconnect_counter_, 1);
}

TEST_F(ConnectionTokenAttestationTest, AttestationFailed) {
  base::HistogramTester histogram_tester;

  CreateConnectionAttestation();

  // No requests sent until token is provided.
  EXPECT_EQ(fake_connection_->pending_requests().size(), 0u);

  token_manager_.RunPendingCallbacks();

  // Respond to attestation request with error.
  ASSERT_EQ(fake_connection_->pending_requests().size(), 1u);
  auto requests = std::move(fake_connection_->pending_requests());
  std::move(requests[0].callback).Run(base::unexpected(ErrorCode::kError));

  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;
  connection_attestation_->Send(proto::PrivateAiRequest(), base::Seconds(1),
                                future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kClientAttestationFailed);

  histogram_tester.ExpectUniqueSample("PrivateAi.Client.RequestErrorCode",
                                      ErrorCode::kError, 1);

  EXPECT_EQ(on_disconnect_counter_, 1);
}

}  // namespace

}  // namespace private_ai
