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
using testing::_;

using MockAuthResultCallback =
    base::MockCallback<PasswordAccessAuthenticator::AuthResultCallback>;
using MockReauthCallback =
    base::MockCallback<PasswordAccessAuthenticator::ReauthCallback>;
using MockTimeoutCallback =
    base::MockCallback<PasswordAccessAuthenticator::TimeoutCallback>;

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
  MockTimeoutCallback& timeout_callback() { return timeout_callback_; }
  PasswordAccessAuthenticator& authenticator() { return authenticator_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  MockAuthResultCallback result_callback_;
  MockReauthCallback os_reauth_callback_;
  MockTimeoutCallback timeout_callback_;
  PasswordAccessAuthenticator authenticator_{os_reauth_callback_.Get(),
                                             timeout_callback_.Get()};
};

// Check that a passed authentication does not expire before kAuthValidityPeriod
// and does expire after kAuthValidityPeriod.
TEST_P(PasswordAccessAuthenticatorTest, Expiration) {
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), _))
      .WillOnce(testing::WithArg<1>(
          [](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(result_callback(), Run(true));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       1);

  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod - base::Seconds(1));
  EXPECT_CALL(os_reauth_callback(), Run).Times(0);
  EXPECT_CALL(timeout_callback(), Run).Times(0);
  EXPECT_CALL(result_callback(), Run(true));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSkipped,
                                       1);

  EXPECT_CALL(timeout_callback(), Run);
  task_environment().FastForwardBy(base::Seconds(2));
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), _))
      .WillOnce(testing::WithArg<1>(
          [](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(result_callback(), Run(true));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       2);
}

// Check that a forced authentication ignores previous successful challenges.
TEST_P(PasswordAccessAuthenticatorTest, ForceReauth) {
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), _))
      .WillOnce(testing::WithArg<1>(
          [](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(result_callback(), Run(true));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       1);

  EXPECT_CALL(os_reauth_callback(), Run(purpose(), _))
      .WillOnce(testing::WithArg<1>(
          [](PasswordAccessAuthenticator::AuthResultCallback callback) {
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
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), _))
      .WillOnce(testing::WithArg<1>(
          [](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(false);
          }));
  EXPECT_CALL(result_callback(), Run(false));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kFailure,
                                       1);

  // Advance just a little bit, so that if |authenticator| starts the grace
  // period, this is still within it.
  task_environment().FastForwardBy(base::Seconds(1));
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), _))
      .WillOnce(testing::WithArg<1>(
          [](PasswordAccessAuthenticator::AuthResultCallback callback) {
            std::move(callback).Run(false);
          }));
  EXPECT_CALL(result_callback(), Run(false));
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kFailure,
                                       2);

  EXPECT_CALL(timeout_callback(), Run).Times(0);
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod);
}

// Check that measurement of time it takes user to authenticate is correct and
// that when the time from the last successful authentication is smaller than
// kAuthValidityPeriod we don't force reauthentication.
TEST_P(PasswordAccessAuthenticatorTest,
       AuthenticationTimeMetricWithValidityPeriod) {
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), _))
      .WillOnce(testing::WithArg<1>(
          [this](PasswordAccessAuthenticator::AuthResultCallback callback) {
            // Waiting for 10 seconds to simulate the time user will need to
            // authenticate, any other not zero time would also do.
            task_environment().FastForwardBy(base::Seconds(10));
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(result_callback(), Run(true)).Times(2);

  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.Settings.AuthenticationTime", base::Seconds(10), 1);

  // Simulatiing time between authentications.
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod / 2);

  // Wait time is smaller than the kAuthValidityPeriod so we shouldn't prompt
  // user for reauthentication. Only one authentication should be recorded.
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.Settings.AuthenticationTime", base::Seconds(10), 1);
}

// Check that measurement of time it takes user to authenticate is correct and
// that when the time from the last successful authentication is larger than
// kAuthValidityPeriod we force reauthentication and measure its time
// correctly.
TEST_P(PasswordAccessAuthenticatorTest, AuthenticationTimeMetric) {
  EXPECT_CALL(os_reauth_callback(), Run(purpose(), _))
      .Times(2)
      .WillRepeatedly(testing::WithArg<1>(
          [this](PasswordAccessAuthenticator::AuthResultCallback callback) {
            // Waiting for 10 seconds to simulate the time user will need to
            // authenticate, any other not zero time would also do.
            task_environment().FastForwardBy(base::Seconds(10));
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(result_callback(), Run(true)).Times(2);

  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.Settings.AuthenticationTime", base::Seconds(10), 1);

  // Additional wait to ensure reauthenticating.
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod * 2);

  // Because waiting time is longer than the kAuthValidityPeriod(60 sec.) User
  // will have to reauthenticate, so we expect 2 samples in the bucket.
  authenticator().EnsureUserIsAuthenticated(purpose(), result_callback().Get());
  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.Settings.AuthenticationTime", base::Seconds(10), 2);
}

INSTANTIATE_TEST_SUITE_P(,
                         PasswordAccessAuthenticatorTest,
                         Values(ReauthPurpose::VIEW_PASSWORD,
                                ReauthPurpose::COPY_PASSWORD,
                                ReauthPurpose::EDIT_PASSWORD,
                                ReauthPurpose::EXPORT));

}  // namespace password_manager
