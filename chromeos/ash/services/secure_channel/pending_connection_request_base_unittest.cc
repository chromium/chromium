// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/pending_connection_request_base.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/fake_connection_delegate.h"
#include "chromeos/ash/services/secure_channel/fake_pending_connection_request_delegate.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const char kTestReadableRequestTypeForLogging[] = "Test Request Type";
const char kTestFeature[] = "testFeature";
const mojom::ConnectionAttemptFailureReason kTestFailureReason =
    mojom::ConnectionAttemptFailureReason::TIMEOUT_FINDING_DEVICE;

enum class TestFailureDetail {
  kReasonWhichCausesRequestToBecomeInactive,
  kReasonWhichDoesNotCauseRequestToBecomeInactive
};

// Since PendingConnectionRequestBase is templatized, a concrete implementation
// is needed for its test.
class TestPendingConnectionRequest
    : public PendingConnectionRequestBase<TestFailureDetail> {
 public:
  TestPendingConnectionRequest(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      PendingConnectionRequestDelegate* delegate)
      : PendingConnectionRequestBase<TestFailureDetail>(
            std::move(client_connection_parameters),
            connection_priority,
            kTestReadableRequestTypeForLogging,
            delegate) {}
  ~TestPendingConnectionRequest() override = default;

  // PendingConnectionRequestBase<TestFailureDetail>:
  void HandleConnectionFailure(TestFailureDetail failure_detail) override {
    switch (failure_detail) {
      case TestFailureDetail::kReasonWhichCausesRequestToBecomeInactive:
        StopRequestDueToConnectionFailures(kTestFailureReason);
        break;
      case TestFailureDetail::kReasonWhichDoesNotCauseRequestToBecomeInactive:
        // Do nothing.
        break;
    }
  }
};

}  // namespace

class SecureChannelPendingConnectionRequestBaseTest : public testing::Test {
 public:
  SecureChannelPendingConnectionRequestBaseTest(
      const SecureChannelPendingConnectionRequestBaseTest&) = delete;
  SecureChannelPendingConnectionRequestBaseTest& operator=(
      const SecureChannelPendingConnectionRequestBaseTest&) = delete;

 protected:
  SecureChannelPendingConnectionRequestBaseTest() = default;
  ~SecureChannelPendingConnectionRequestBaseTest() override = default;

  void SetUp() override {
    auto fake_client_connection_parameters =
        std::make_unique<FakeClientConnectionParameters>(kTestFeature);
    fake_client_connection_parameters_ =
        fake_client_connection_parameters.get();

    fake_pending_connection_request_delegate_ =
        std::make_unique<FakePendingConnectionRequestDelegate>();

    test_pending_connection_request_ =
        std::make_unique<TestPendingConnectionRequest>(
            std::move(fake_client_connection_parameters),
            ConnectionPriority::kLow,
            fake_pending_connection_request_delegate_.get());
  }

  void HandleConnectionFailure(TestFailureDetail test_failure_detail) {
    test_pending_connection_request_->HandleConnectionFailure(
        test_failure_detail);
  }

  const std::optional<PendingConnectionRequestDelegate::FailedConnectionReason>&
  GetFailedConnectionReason() {
    return fake_pending_connection_request_delegate_
        ->GetFailedConnectionReasonForId(
            test_pending_connection_request_->GetRequestId());
  }

  void CancelClientRequest() {
    fake_client_connection_parameters_->CancelClientRequest();
  }

  const std::optional<mojom::ConnectionAttemptFailureReason>&
  GetConnectionAttemptFailureReason() const {
    return fake_client_connection_parameters_->failure_reason();
  }

  std::unique_ptr<ClientConnectionParameters>
  ExtractClientConnectionParameters() {
    return PendingConnectionRequest<TestFailureDetail>::
        ExtractClientConnectionParameters(
            std::move(test_pending_connection_request_));
  }

  FakeClientConnectionParameters* fake_client_connection_parameters() {
    return fake_client_connection_parameters_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  raw_ptr<FakeClientConnectionParameters, DanglingUntriaged>
      fake_client_connection_parameters_;
  std::unique_ptr<FakePendingConnectionRequestDelegate>
      fake_pending_connection_request_delegate_;

  std::unique_ptr<TestPendingConnectionRequest>
      test_pending_connection_request_;
};

TEST_F(SecureChannelPendingConnectionRequestBaseTest,
       HandleConnectionFailureWhichCausesRequestToBecomeInactive) {
  HandleConnectionFailure(
      TestFailureDetail::kReasonWhichCausesRequestToBecomeInactive);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(kTestFailureReason, *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingConnectionRequestBaseTest,
       HandleConnectionFailureWhichDoesNotCauseRequestToBecomeInactive) {
  // Repeat 5 connection failures, none of which should cause the request to
  // become inactive.
  for (int i = 0; i < 5; ++i) {
    HandleConnectionFailure(
        TestFailureDetail::kReasonWhichDoesNotCauseRequestToBecomeInactive);
    EXPECT_FALSE(GetFailedConnectionReason());
    EXPECT_FALSE(GetConnectionAttemptFailureReason());
  }
}

TEST_F(SecureChannelPendingConnectionRequestBaseTest, ClientCancelsRequest) {
  CancelClientRequest();
  EXPECT_EQ(PendingConnectionRequestDelegate::FailedConnectionReason::
                kRequestCanceledByClient,
            *GetFailedConnectionReason());
}

TEST_F(SecureChannelPendingConnectionRequestBaseTest,
       ExtractClientConnectionParameters) {
  auto extracted_client_data = ExtractClientConnectionParameters();
  EXPECT_EQ(fake_client_connection_parameters(), extracted_client_data.get());
}

}  // namespace ash::secure_channel
