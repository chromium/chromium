// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_session_durations_metrics_recorder.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/driver/test_sync_service.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class SyncSessionDurationsMetricsRecorderTest : public testing::Test {
 public:
  SyncSessionDurationsMetricsRecorderTest()
      : identity_test_env_(&test_url_loader_factory_),
        session_time_(base::TimeDelta::FromSeconds(10)) {
    sync_service_.SetIsAuthenticatedAccountPrimary(false);
    sync_service_.SetDisableReasons(SyncService::DISABLE_REASON_NOT_SIGNED_IN);
  }

  ~SyncSessionDurationsMetricsRecorderTest() override {}

  void EnableSync() {
    identity_test_env_.MakePrimaryAccountAvailable("foo@gmail.com");
    sync_service_.SetIsAuthenticatedAccountPrimary(true);
    sync_service_.SetDisableReasons(SyncService::DISABLE_REASON_NONE);
  }

  void SetInvalidCredentialsAuthError() {
    GoogleServiceAuthError auth_error(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_test_env_.identity_manager()->GetPrimaryAccountId(),
        auth_error);
    sync_service_.SetAuthError(auth_error);
  }

  std::string GetSessionHistogramName(const std::string& histogram_suffix) {
    return std::string("Session.TotalDuration.") + histogram_suffix;
  }

  void ExpectOneSession(const base::HistogramTester& ht,
                        const std::vector<std::string>& histogram_suffixes) {
    for (const auto& histogram_suffix : histogram_suffixes) {
      ht.ExpectTimeBucketCount(GetSessionHistogramName(histogram_suffix),
                               session_time_, 1);
    }
  }

  void ExpectNoSession(const base::HistogramTester& ht,
                       const std::vector<std::string>& histogram_suffixes) {
    for (const auto& histogram_suffix : histogram_suffixes) {
      ht.ExpectTotalCount(GetSessionHistogramName(histogram_suffix), 0);
    }
  }

  void StartAndEndSession() {
    SyncSessionDurationsMetricsRecorder metrics_recorder(
        &sync_service_, identity_test_env_.identity_manager());
    metrics_recorder.OnSessionStarted(base::TimeTicks::Now());
    metrics_recorder.OnSessionEnded(session_time_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  TestSyncService sync_service_;
  const base::TimeDelta session_time_;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionDurationsMetricsRecorderTest);
};

TEST_F(SyncSessionDurationsMetricsRecorderTest, WebSignedOut) {
  base::HistogramTester ht;
  StartAndEndSession();

  ExpectOneSession(ht, {"WithoutAccount"});
  ExpectNoSession(ht, {"WithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest, WebSignedIn) {
  identity_test_env_.SetCookieAccounts({{"foo@gmail.com", "foo_gaia_id"}});

  base::HistogramTester ht;
  StartAndEndSession();

  ExpectOneSession(ht, {"WithAccount"});
  ExpectNoSession(ht, {"WithoutAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest, NotOptedInToSync) {
  base::HistogramTester ht;
  StartAndEndSession();

  ExpectOneSession(ht, {"NotOptedInToSyncWithoutAccount"});
  ExpectNoSession(ht,
                  {"NotOptedInToSyncWithAccount", "OptedInToSyncWithoutAccount",
                   "OptedInToSyncWithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest, OptedInToSync_SyncActive) {
  EnableSync();

  base::HistogramTester ht;
  StartAndEndSession();

  ExpectOneSession(ht, {"OptedInToSyncWithAccount"});
  ExpectNoSession(
      ht, {"NotOptedInToSyncWithoutAccount", "NotOptedInToSyncWithoutAccount",
           "OptedInToSyncWithoutAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest,
       OptedInToSync_SyncDisabledByUser) {
  EnableSync();
  sync_service_.SetDisableReasons(SyncService::DISABLE_REASON_USER_CHOICE);

  base::HistogramTester ht;
  StartAndEndSession();

  // If the user opted in to sync, but then disabled sync (e.g. via policy or
  // from the Android OS settings), then they are counted as having opted out
  // of sync.
  ExpectOneSession(ht, {"NotOptedInToSyncWithAccount"});
  ExpectNoSession(
      ht, {"NotOptedInToSyncWithoutAccount", "OptedInToSyncWithoutAccount",
           "OptedInToSyncWithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest,
       OptedInToSync_PrimaryAccountInAuthError) {
  EnableSync();
  SetInvalidCredentialsAuthError();

  base::HistogramTester ht;
  StartAndEndSession();

  ExpectOneSession(ht, {"OptedInToSyncWithoutAccount"});
  ExpectNoSession(
      ht, {"NotOptedInToSyncWithoutAccount", "NotOptedInToSyncWithoutAccount",
           "OptedInToSyncWithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest,
       SyncDisabled_PrimaryAccountInAuthError) {
  EnableSync();
  SetInvalidCredentialsAuthError();
  sync_service_.SetDisableReasons(SyncService::DISABLE_REASON_USER_CHOICE);

  base::HistogramTester ht;
  StartAndEndSession();

  // If the user opted in to sync, but then disabled sync (e.g. via policy or
  // from the Android OS settings), then they are counted as having opted out
  // of sync.
  // The account is in auth error, so they are also counted as not having any
  // browser account.
  ExpectOneSession(ht, {"NotOptedInToSyncWithoutAccount"});
  ExpectNoSession(ht,
                  {"NotOptedInToSyncWithAccount", "OptedInToSyncWithoutAccount",
                   "OptedInToSyncWithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest,
       NotOptedInToSync_AccountInAuthError) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("foo@gmail.com");
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  base::HistogramTester ht;
  StartAndEndSession();

  // The account is in auth error, so they are counted as not having any browser
  // account.
  ExpectOneSession(ht, {"NotOptedInToSyncWithoutAccount"});
  ExpectNoSession(ht,
                  {"NotOptedInToSyncWithAccount", "OptedInToSyncWithoutAccount",
                   "OptedInToSyncWithAccount"});
}

}  // namespace
}  // namespace syncer
