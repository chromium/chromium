// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/user_security_signals_service.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

using testing::_;
using testing::Mock;

namespace {

class MockUserSecuritySignalsServiceDelegate
    : public UserSecuritySignalsService::Delegate {
 public:
  MockUserSecuritySignalsServiceDelegate() = default;
  ~MockUserSecuritySignalsServiceDelegate() = default;

  MOCK_METHOD(void,
              OnReportEventTriggered,
              (SecurityReportTrigger),
              (override));
};

}  // namespace

class UserSecuritySignalsServiceTest : public testing::Test {
 protected:
  UserSecuritySignalsServiceTest() {
    testing_prefs_.registry()->RegisterBooleanPref(
        kUserSecuritySignalsReporting, false);
    testing_prefs_.registry()->RegisterBooleanPref(
        kUserSecurityAuthenticatedReporting, false);
  }

  void SetUp() override {
    ON_CALL(delegate_, OnReportEventTriggered(_))
        .WillByDefault(
            [&](SecurityReportTrigger) { service_->OnReportUploaded(); });
  }

  void SetEnabledPolicy(bool enabled) {
    testing_prefs_.SetBoolean(kUserSecuritySignalsReporting, enabled);
  }

  void SetUseAuthPolicy(bool use_auth) {
    testing_prefs_.SetBoolean(kUserSecurityAuthenticatedReporting, use_auth);
  }

  void CreateUserSecuritySignalsService(bool start_service = false) {
    service_ = std::make_unique<UserSecuritySignalsService>(&testing_prefs_,
                                                            &delegate_);

    if (start_service) {
      service_->Start();
      task_environment_.RunUntilIdle();
    }
  }

  void FastForwardTimeToTrigger() {
    task_environment_.FastForwardBy(
        UserSecuritySignalsService::GetSecurityUploadCadence());
  }

  void FastForwardByHalfTimeToTrigger() {
    // Adding a second to remove rounding errors.
    task_environment_.FastForwardBy(
        UserSecuritySignalsService::GetSecurityUploadCadence() / 2 +
        base::Seconds(1));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple testing_prefs_;
  std::unique_ptr<UserSecuritySignalsService> service_ = nullptr;
  testing::StrictMock<MockUserSecuritySignalsServiceDelegate> delegate_;
};

TEST_F(UserSecuritySignalsServiceTest, NotStarted) {
  CreateUserSecuritySignalsService();

  EXPECT_FALSE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_FALSE(service_->ShouldUseCookies());

  SetEnabledPolicy(true);
  SetUseAuthPolicy(true);

  // Pref values should be available even if the service was not started.
  EXPECT_TRUE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_TRUE(service_->ShouldUseCookies());

  // No trigger should occur even if we fast forward.
  FastForwardTimeToTrigger();
}

TEST_F(UserSecuritySignalsServiceTest, PolicyDefault) {
  EXPECT_CALL(delegate_, OnReportEventTriggered(_)).Times(0);

  CreateUserSecuritySignalsService(/*start_service=*/true);

  EXPECT_FALSE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_FALSE(service_->ShouldUseCookies());

  // No trigger should occur even if we fast forward.
  FastForwardTimeToTrigger();
}

TEST_F(UserSecuritySignalsServiceTest, PolicyEnabledWithoutCookies) {
  SetEnabledPolicy(true);

  // Creation of the service with the pref value already enabled will trigger an
  // upload.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);

  CreateUserSecuritySignalsService(/*start_service=*/true);

  EXPECT_TRUE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_FALSE(service_->ShouldUseCookies());
}

TEST_F(UserSecuritySignalsServiceTest, PolicyEnabledWithCookies_FastForwards) {
  SetEnabledPolicy(true);
  SetUseAuthPolicy(true);

  // A upload should occur first on service creation.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);

  CreateUserSecuritySignalsService(/*start_service=*/true);

  EXPECT_TRUE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_TRUE(service_->ShouldUseCookies());

  // Fast forwarding should trigger another upload.
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  FastForwardTimeToTrigger();

  // Fast forwarding again should trigger another upload.
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  FastForwardTimeToTrigger();
}

// Test case to simulate when a security signals report is uploaded by a
// different service. For example, Chrome profile reporting can send a full
// report with security signals on its own schedule.
TEST_F(UserSecuritySignalsServiceTest,
       PolicyEnabledWithCookies_ExternalTriggerDelays) {
  SetEnabledPolicy(true);
  SetUseAuthPolicy(true);

  // A upload should occur first on service creation.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  CreateUserSecuritySignalsService(/*start_service=*/true);

  EXPECT_TRUE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_TRUE(service_->ShouldUseCookies());

  // Signaling that a report was uploaded after waiting a halftime means waiting
  // another halftime should not result in a second report being triggered.
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(0);
  FastForwardByHalfTimeToTrigger();
  service_->OnReportUploaded();
  FastForwardByHalfTimeToTrigger();
}

TEST_F(UserSecuritySignalsServiceTest, PolicyBecomesEnabledWithoutCookies) {
  CreateUserSecuritySignalsService(/*start_service=*/true);
  EXPECT_FALSE(service_->IsSecuritySignalsReportingEnabled());
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(0);

  // No trigger should occur even if we fast forward.
  FastForwardTimeToTrigger();
  Mock::VerifyAndClearExpectations(&delegate_);

  // A report should be triggered when the policy becomes enabled.
  EXPECT_CALL(delegate_, OnReportEventTriggered(SecurityReportTrigger::kTimer))
      .Times(1);
  SetEnabledPolicy(true);
  task_environment_.RunUntilIdle();
}

}  // namespace enterprise_reporting
