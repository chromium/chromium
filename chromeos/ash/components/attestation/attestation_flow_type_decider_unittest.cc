// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow_type_decider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/attestation/attestation_flow_status_reporter.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace ash {
namespace attestation {

class AttestationFlowTypeDeciderTest : public testing::Test {
 public:
  AttestationFlowTypeDeciderTest() = default;
  ~AttestationFlowTypeDeciderTest() override = default;

 protected:
  // Checks if the expectation of the statuses in the reporter is working
  // correctly, despite of the fact that the object under test doesn't call
  // `Report()`.
  void ExpectReport(bool has_proxy) {
    base::HistogramTester histogram_tester;
    // Complete the UMA flag setup before reporting UMA.
    reporter_->OnFallbackFlowStatus(false);
    // Reset the reporter to trigger UMA reporting.
    reporter_.reset();
    // (1<<1) is set by `OnFallbackFlowStatus()`, and (1<<5) is set if having
    // proxy.
    const int expected_entry = (1 << 1) | (has_proxy ? (1 << 5) : 0);
    histogram_tester.ExpectUniqueSample(
        "ChromeOS.Attestation.AttestationFlowStatus", expected_entry, 1);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  StrictMock<MockServerProxy> server_proxy_;
  std::unique_ptr<AttestationFlowStatusReporter> reporter_;
};

// The integrated flow is valid when there is no proxy.
TEST_F(AttestationFlowTypeDeciderTest,
       IntegratedFlowPossibleWithNoProxyPresent) {
  bool result = false;
  auto callback = [](base::RunLoop* run_loop, bool* result, bool is_valid) {
    *result = is_valid;
    run_loop->QuitClosure().Run();
  };
  base::RunLoop run_loop;

  EXPECT_CALL(server_proxy_, CheckIfAnyProxyPresent(_))
      .WillOnce(Invoke([](ServerProxy::ProxyPresenceCallback callback) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), false));
      }));
  reporter_ = std::make_unique<AttestationFlowStatusReporter>();
  AttestationFlowTypeDecider checker;
  checker.CheckType(&server_proxy_, reporter_.get(),
                    base::BindOnce(callback, &run_loop, &result));
  run_loop.Run();
  EXPECT_TRUE(result);
  ExpectReport(/*has_proxy=*/false);
}

// The integrated flow is invalid when there is a proxy.
TEST_F(AttestationFlowTypeDeciderTest,
       IntegratedFlowImpossibleWithProxyPresent) {
  bool result = true;
  auto callback = [](base::RunLoop* run_loop, bool* result, bool is_valid) {
    *result = is_valid;
    run_loop->QuitClosure().Run();
  };
  base::RunLoop run_loop;

  EXPECT_CALL(server_proxy_, CheckIfAnyProxyPresent(_))
      .WillOnce(Invoke([](ServerProxy::ProxyPresenceCallback callback) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      }));
  reporter_ = std::make_unique<AttestationFlowStatusReporter>();
  AttestationFlowTypeDecider checker;
  checker.CheckType(&server_proxy_, reporter_.get(),
                    base::BindOnce(callback, &run_loop, &result));
  run_loop.Run();
  EXPECT_FALSE(result);
  ExpectReport(/*has_proxy=*/true);
}

}  // namespace attestation
}  // namespace ash
