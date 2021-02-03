// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/attestation_flow_type_decider.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace chromeos {
namespace attestation {

class AttestationFlowTypeDeciderTest : public testing::Test {
 public:
  AttestationFlowTypeDeciderTest() = default;
  ~AttestationFlowTypeDeciderTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  StrictMock<MockServerProxy> server_proxy_;
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
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), false));
      }));
  AttestationFlowTypeDecider checker;
  checker.CheckType(&server_proxy_,
                    base::BindOnce(callback, &run_loop, &result));
  run_loop.Run();
  EXPECT_TRUE(result);
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
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      }));
  AttestationFlowTypeDecider checker;
  checker.CheckType(&server_proxy_,
                    base::BindOnce(callback, &run_loop, &result));
  run_loop.Run();
  EXPECT_FALSE(result);
}

}  // namespace attestation
}  // namespace chromeos
