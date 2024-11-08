// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_service.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/enterprise_companion/dm_client.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/event_logger.h"
#include "chrome/enterprise_companion/telemetry_logger/telemetry_logger.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

namespace {

class MockDMClient final : public DMClient {
 public:
  MockDMClient() = default;
  ~MockDMClient() override = default;

  MOCK_METHOD(void,
              RegisterPolicyAgent,
              (scoped_refptr<EnterpriseCompanionEventLogger> event_logger,
               StatusCallback callback),
              (override));
  MOCK_METHOD(void,
              FetchPolicies,
              (scoped_refptr<EnterpriseCompanionEventLogger> event_logger,
               StatusCallback callback),
              (override));
};

class MockLogger final : public EnterpriseCompanionEventLogger {
 public:
  void LogRegisterPolicyAgentEvent(
      base::Time start_time,
      StatusCallback callback,
      const EnterpriseCompanionStatus& status) override {
    std::move(callback).Run(status);
  }
  void LogPolicyFetchEvent(base::Time start_time,
                           StatusCallback callback,
                           const EnterpriseCompanionStatus& status) override {
    std::move(callback).Run(status);
  }
  void Flush(base::OnceClosure callback) override { std::move(callback).Run(); }

 protected:
  ~MockLogger() override = default;
};

}  // namespace

class EnterpriseCompanionServiceTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment environment_;
};

TEST_F(EnterpriseCompanionServiceTest, Shutdown) {
  base::MockCallback<base::OnceClosure> shutdown_callback;
  base::RunLoop service_run_loop;
  std::unique_ptr<EnterpriseCompanionService> service =
      CreateEnterpriseCompanionService(std::make_unique<MockDMClient>(),
                                       base::MakeRefCounted<MockLogger>(),
                                       service_run_loop.QuitClosure());

  EXPECT_CALL(shutdown_callback, Run()).Times(1);
  service->Shutdown(shutdown_callback.Get());
  service_run_loop.Run(FROM_HERE);
}

TEST_F(EnterpriseCompanionServiceTest, FetchPoliciesSuccess) {
  std::unique_ptr<MockDMClient> mock_dm_client_ =
      std::make_unique<MockDMClient>();
  EXPECT_CALL(*mock_dm_client_, RegisterPolicyAgent)
      .WillOnce([](scoped_refptr<EnterpriseCompanionEventLogger>,
                   StatusCallback callback) {
        std::move(callback).Run(EnterpriseCompanionStatus::Success());
      });
  EXPECT_CALL(*mock_dm_client_, FetchPolicies)
      .WillOnce([](scoped_refptr<EnterpriseCompanionEventLogger>,
                   StatusCallback callback) {
        std::move(callback).Run(EnterpriseCompanionStatus::Success());
      });

  std::unique_ptr<EnterpriseCompanionService> service =
      CreateEnterpriseCompanionService(std::move(mock_dm_client_),
                                       base::MakeRefCounted<MockLogger>(),
                                       base::DoNothing());

  base::RunLoop run_loop;
  service->FetchPolicies(
      base::BindOnce([](const EnterpriseCompanionStatus& status) {
        EXPECT_TRUE(status.ok());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(EnterpriseCompanionServiceTest, FetchPoliciesRegistrationFail) {
  std::unique_ptr<MockDMClient> mock_dm_client_ =
      std::make_unique<MockDMClient>();
  EXPECT_CALL(*mock_dm_client_, RegisterPolicyAgent)
      .WillOnce([](scoped_refptr<EnterpriseCompanionEventLogger>,
                   StatusCallback callback) {
        std::move(callback).Run(
            EnterpriseCompanionStatus::FromDeviceManagementStatus(
                policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND));
      });
  EXPECT_CALL(*mock_dm_client_, FetchPolicies).Times(0);

  std::unique_ptr<EnterpriseCompanionService> service =
      CreateEnterpriseCompanionService(std::move(mock_dm_client_),
                                       base::MakeRefCounted<MockLogger>(),
                                       base::DoNothing());

  base::RunLoop run_loop;
  service->FetchPolicies(
      base::BindOnce([](const EnterpriseCompanionStatus& status) {
        EXPECT_TRUE(status.EqualsDeviceManagementStatus(
            policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND));
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(EnterpriseCompanionServiceTest, FetchPoliciesFail) {
  std::unique_ptr<MockDMClient> mock_dm_client_ =
      std::make_unique<MockDMClient>();
  EXPECT_CALL(*mock_dm_client_, RegisterPolicyAgent)
      .WillOnce([](scoped_refptr<EnterpriseCompanionEventLogger>,
                   StatusCallback callback) {
        std::move(callback).Run(EnterpriseCompanionStatus::Success());
      });
  EXPECT_CALL(*mock_dm_client_, FetchPolicies)
      .WillOnce([](scoped_refptr<EnterpriseCompanionEventLogger>,
                   StatusCallback callback) {
        std::move(callback).Run(
            EnterpriseCompanionStatus::FromDeviceManagementStatus(
                policy::DM_STATUS_HTTP_STATUS_ERROR));
      });

  std::unique_ptr<EnterpriseCompanionService> service =
      CreateEnterpriseCompanionService(std::move(mock_dm_client_),
                                       base::MakeRefCounted<MockLogger>(),
                                       base::DoNothing());

  base::RunLoop run_loop;
  service->FetchPolicies(
      base::BindOnce([](const EnterpriseCompanionStatus& status) {
        EXPECT_TRUE(status.EqualsDeviceManagementStatus(
            policy::DM_STATUS_HTTP_STATUS_ERROR));
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace enterprise_companion
