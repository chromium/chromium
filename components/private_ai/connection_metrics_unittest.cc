// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_metrics.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/testing/fake_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

class ConnectionMetricsTest : public testing::Test {
 public:
  ConnectionMetricsTest() {
    auto fake_connection = std::make_unique<FakeConnection>(base::DoNothing());
    fake_connection_ = fake_connection.get();
    connection_metrics_ =
        std::make_unique<ConnectionMetrics>(std::move(fake_connection));
  }

  ~ConnectionMetricsTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<ConnectionMetrics> connection_metrics_;
  raw_ptr<FakeConnection> fake_connection_;

  base::HistogramTester histogram_tester_;
};

TEST_F(ConnectionMetricsTest, Success) {
  // Prepare request.
  proto::PrivateAiRequest request;
  request.set_feature_name(
      proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT);
  request.mutable_generate_content_request()->set_model("test-model");

  const size_t request_size = request.ByteSizeLong();

  // Send request.
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;
  connection_metrics_->Send(std::move(request), base::Seconds(10),
                            future.GetCallback());

  // Prepare response.
  proto::PrivateAiResponse response;
  response.set_request_id(123);
  const size_t response_size = response.ByteSizeLong();

  // Send response.
  task_environment_.FastForwardBy(base::Milliseconds(500));

  ASSERT_EQ(fake_connection_->pending_requests().size(), 1u);
  std::move(fake_connection_->pending_requests()[0].callback)
      .Run(std::move(response));

  // Receive result.
  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().request_id(), 123);

  // Verify request related metrics.
  histogram_tester_.ExpectUniqueSample("PrivateAi.Client.RequestSize",
                                       request_size, 1);
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.Client.FeatureName",
      proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT, 1);

  // Verify response related metrics.
  histogram_tester_.ExpectUniqueSample("PrivateAi.Client.ResponseSize.Success",
                                       response_size, 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "PrivateAi.Client.RequestLatency.Success", base::Milliseconds(500), 1);
  histogram_tester_.ExpectTotalCount("PrivateAi.Client.RequestErrorCode", 0);
}

TEST_F(ConnectionMetricsTest, Timeout) {
  // Send request.
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;
  connection_metrics_->Send(proto::PrivateAiRequest(), base::Seconds(10),
                            future.GetCallback());

  // Send response.
  task_environment_.FastForwardBy(base::Seconds(10));

  ASSERT_EQ(fake_connection_->pending_requests().size(), 1u);
  std::move(fake_connection_->pending_requests()[0].callback)
      .Run(base::unexpected(ErrorCode::kTimeout));

  // Receive result.
  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kTimeout);

  // Verify response related metrics.
  histogram_tester_.ExpectUniqueSample("PrivateAi.Client.RequestErrorCode",
                                       ErrorCode::kTimeout, 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "PrivateAi.Client.RequestLatency.Timeout", base::Seconds(10), 1);
  histogram_tester_.ExpectTotalCount("PrivateAi.Client.RequestLatency.Success",
                                     0);
  histogram_tester_.ExpectTotalCount("PrivateAi.Client.RequestLatency.Error",
                                     0);
}

TEST_F(ConnectionMetricsTest, Error) {
  // Send request.
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;
  connection_metrics_->Send(proto::PrivateAiRequest(), base::Seconds(10),
                            future.GetCallback());

  // Send response.

  task_environment_.FastForwardBy(base::Milliseconds(200));

  ASSERT_EQ(fake_connection_->pending_requests().size(), 1u);
  std::move(fake_connection_->pending_requests()[0].callback)
      .Run(base::unexpected(ErrorCode::kNetworkError));

  // Receive result.
  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kNetworkError);

  // Verify response related metrics.
  histogram_tester_.ExpectUniqueSample("PrivateAi.Client.RequestErrorCode",
                                       ErrorCode::kNetworkError, 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "PrivateAi.Client.RequestLatency.Error", base::Milliseconds(200), 1);
  histogram_tester_.ExpectTotalCount("PrivateAi.Client.RequestLatency.Success",
                                     0);
  histogram_tester_.ExpectTotalCount("PrivateAi.Client.RequestLatency.Timeout",
                                     0);
}

}  // namespace

}  // namespace private_ai
