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
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
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

  // Both attestation and buffered requests should be sent immediately.
  ASSERT_EQ(fake_connection_->pending_requests().size(), 2u);
  {
    const auto& pending_request =
        fake_connection_->pending_requests()[0].request;
    ASSERT_TRUE(pending_request.has_anonymous_token_request());
    EXPECT_EQ(pending_request.feature_name(),
              proto::FeatureName::FEATURE_NAME_CHROME_CLIENT_ATTESTATION);
    privacy::ppn::PrivacyPassTokenData expected_token_data;
    expected_token_data.set_token(FakeTokenManager::kFakeToken);
    expected_token_data.set_encoded_extensions("test_extensions");
    EXPECT_EQ(pending_request.anonymous_token_request().anonymous_token(),
              expected_token_data.SerializeAsString());
  }

  EXPECT_EQ(fake_connection_->pending_requests()[1].request.request_id(), 123);

  // Respond to the original request.
  proto::PrivateAiResponse response;
  response.set_request_id(123);
  auto cb = std::move(fake_connection_->pending_requests()[1].callback);
  fake_connection_->pending_requests()[1].callback = base::DoNothing();
  std::move(cb).Run(std::move(response));

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

TEST_F(ConnectionTokenAttestationTest, ErrorBeforeFirstResponse) {
  CreateConnectionAttestation();

  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;
  proto::PrivateAiRequest request;
  request.set_request_id(123);
  connection_attestation_->Send(std::move(request), base::Seconds(1),
                                future.GetCallback());

  // Provide the token.
  token_manager_.RunPendingCallbacks();

  ASSERT_EQ(fake_connection_->pending_requests().size(), 2u);

  // Simulate any error before a successful response.
  auto cb = std::move(fake_connection_->pending_requests()[1].callback);
  fake_connection_->pending_requests()[1].callback = base::DoNothing();
  std::move(cb).Run(base::unexpected(ErrorCode::kNetworkError));

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  // The error should be rewritten to kClientAttestationFailed
  EXPECT_EQ(result.error(), ErrorCode::kClientAttestationFailed);

  // We expect a disconnect to be requested.
  EXPECT_EQ(on_disconnect_counter_, 1);
}

TEST_F(ConnectionTokenAttestationTest, ErrorAfterFirstResponse) {
  CreateConnectionAttestation();

  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future1;
  proto::PrivateAiRequest request1;
  request1.set_request_id(1);
  connection_attestation_->Send(std::move(request1), base::Seconds(1),
                                future1.GetCallback());

  // Provide the token.
  token_manager_.RunPendingCallbacks();

  ASSERT_EQ(fake_connection_->pending_requests().size(), 2u);

  // Respond successfully to the first request.
  proto::PrivateAiResponse response;
  response.set_request_id(1);
  auto cb1 = std::move(fake_connection_->pending_requests()[1].callback);
  fake_connection_->pending_requests()[1].callback = base::DoNothing();
  std::move(cb1).Run(std::move(response));

  auto result1 = future1.Get();
  ASSERT_TRUE(result1.has_value());

  // Send a second request.
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future2;
  proto::PrivateAiRequest request2;
  request2.set_request_id(2);
  connection_attestation_->Send(std::move(request2), base::Seconds(1),
                                future2.GetCallback());

  // Now an error occurs.
  // The pending_requests() vector has size 3 now (token, request1, request2).
  ASSERT_EQ(fake_connection_->pending_requests().size(), 3u);
  auto cb2 = std::move(fake_connection_->pending_requests()[2].callback);
  fake_connection_->pending_requests()[2].callback = base::DoNothing();
  std::move(cb2).Run(base::unexpected(ErrorCode::kError));

  auto result2 = future2.Get();
  ASSERT_FALSE(result2.has_value());
  // Since we already had a successful response, the error should NOT be
  // rewritten.
  EXPECT_EQ(result2.error(), ErrorCode::kError);

  // No new disconnect call expected directly from attestation heuristic.
  EXPECT_EQ(on_disconnect_counter_, 0);
}

TEST_F(ConnectionTokenAttestationTest, Base64ToWebSafeBase64) {
  EXPECT_EQ(internal::Base64ToWebSafeBase64(""), "");
  EXPECT_EQ(internal::Base64ToWebSafeBase64("abc"), "abc");
  EXPECT_EQ(internal::Base64ToWebSafeBase64("a+b/c"), "a-b_c");
  EXPECT_EQ(internal::Base64ToWebSafeBase64("A+B/C=="), "A-B_C");
  EXPECT_EQ(internal::Base64ToWebSafeBase64("+++///"), "---___");
}

}  // namespace

}  // namespace private_ai
