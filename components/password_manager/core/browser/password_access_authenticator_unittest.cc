// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_access_authenticator.h"

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/reauth_purpose.h"
#include "components/sync/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::TestWithParam;
using ::testing::Values;

namespace password_manager {

using MockTimeoutCallback =
    base::MockCallback<PasswordAccessAuthenticator::TimeoutCallback>;

class PasswordAccessAuthenticatorTest
    : public TestWithParam<std::tuple<ReauthPurpose, bool>> {
 public:
  PasswordAccessAuthenticatorTest() = default;

  ReauthPurpose purpose() { return std::get<0>(GetParam()); }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  MockTimeoutCallback& timeout_callback() { return timeout_callback_; }
  PasswordAccessAuthenticator& authenticator() { return authenticator_; }

 protected:
  void SetUp() override {
    if (std::get<1>(GetParam()))
      feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
    authenticator_.Init(timeout_callback_.Get());
  }

  void TearDown() override { feature_list.Reset(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockTimeoutCallback timeout_callback_;
  PasswordAccessAuthenticator authenticator_;
  base::test::ScopedFeatureList feature_list;
};

TEST_P(PasswordAccessAuthenticatorTest, RestartAuthTimer) {
  // Timeout callback will not be run because the timer was not started.
  authenticator().RestartAuthTimer();
  EXPECT_CALL(timeout_callback(), Run).Times(0);
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::GetAuthValidityPeriod() + base::Seconds(1));

  // Timeout callback will not be run because not enough time has passed.
  authenticator().start_auth_timer(timeout_callback().Get());
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::GetAuthValidityPeriod() - base::Seconds(1));

  // Timeout callback will be run.
  authenticator().RestartAuthTimer();
  EXPECT_CALL(timeout_callback(), Run).Times(1);
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::GetAuthValidityPeriod());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordAccessAuthenticatorTest,
    testing::Combine(testing::Values(ReauthPurpose::VIEW_PASSWORD,
                                     ReauthPurpose::COPY_PASSWORD,
                                     ReauthPurpose::EDIT_PASSWORD,
                                     ReauthPurpose::EXPORT,
                                     ReauthPurpose::IMPORT),
                     testing::Bool()));

}  // namespace password_manager
