// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_access_authenticator.h"

#include <utility>

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/reauth_purpose.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::Values;

namespace password_manager {

using metrics_util::ReauthResult;

using MockAuthResultCallback =
    base::MockCallback<PasswordAccessAuthenticator::AuthResultCallback>;
using MockReauthCallback =
    base::MockCallback<PasswordAccessAuthenticator::ReauthCallback>;

constexpr char kHistogramName[] =
    "PasswordManager.ReauthToAccessPasswordInSettings";

class PasswordAccessAuthenticatorTest : public TestWithParam<ReauthPurpose> {
 public:
  PasswordAccessAuthenticatorTest() = default;

  ReauthPurpose purpose() { return GetParam(); }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  MockAuthResultCallback& result_callback() { return result_callback_; }
  MockReauthCallback& os_reauth_callback() { return os_reauth_callback_; }
  PasswordAccessAuthenticator& authenticator() { return authenticator_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  MockAuthResultCallback result_callback_;
  MockReauthCallback os_reauth_callback_;
  PasswordAccessAuthenticator authenticator_{os_reauth_callback_.Get()};
};

// Check that a passed authentication does not expire before kAuthValidityPeriod
// and does expire after kAuthValidityPeriod.
TEST_P(PasswordAccessAuthenticatorTest, Expiration) {
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), testing::_))
      .WillOnce(testing::WithArg<1>(
          [&](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(result_callback(), Run(true));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       1);

  task_environment().AdvanceClock(
      PasswordAccessAuthenticator::kAuthValidityPeriod - base::Seconds(1));
  EXPECT_CALL(os_reauth_callback(), Run).Times(0);
  EXPECT_CALL(result_callback(), Run(true));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSkipped,
                                       1);

  task_environment().AdvanceClock(base::Seconds(2));
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), testing::_))
      .WillOnce(testing::WithArg<1>(
          [&](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(result_callback(), Run(true));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       2);
}

// Check that a forced authentication ignores previous successful challenges.
TEST_P(PasswordAccessAuthenticatorTest, ForceReauth) {
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), testing::_))
      .WillOnce(testing::WithArg<1>(
          [&](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(result_callback(), Run(true));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       1);

  EXPECT_CALL(os_reauth_callback(), Run(purpose(), testing::_))
      .WillOnce(testing::WithArg<1>(
          [&](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(result_callback(), Run(true));
  authenticator().ForceUserReauthentication(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       2);
}

// Check that a failed authentication does not start the grace period for
// skipping authentication.
TEST_P(PasswordAccessAuthenticatorTest, Failed) {
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), testing::_))
      .WillOnce(testing::WithArg<1>(
          [&](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(false);
          }));
  EXPECT_CALL(result_callback(), Run(false));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kFailure,
                                       1);

  // Advance just a little bit, so that if |authenticator| starts the grace
  // period, this is still within it.
  task_environment().AdvanceClock(base::Seconds(1));
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), testing::_))
      .WillOnce(testing::WithArg<1>(
          [&](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(false);
          }));
  EXPECT_CALL(result_callback(), Run(false));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kFailure,
                                       2);
}

INSTANTIATE_TEST_SUITE_P(,
                         PasswordAccessAuthenticatorTest,
                         Values(ReauthPurpose::VIEW_PASSWORD,
                                ReauthPurpose::COPY_PASSWORD,
                                ReauthPurpose::EDIT_PASSWORD,
                                ReauthPurpose::EXPORT));

}  // namespace password_manager
