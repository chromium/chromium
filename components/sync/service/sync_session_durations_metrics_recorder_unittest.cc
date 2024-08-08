// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_session_durations_metrics_recorder.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

constexpr base::TimeDelta kSessionTime = base::Seconds(10);

class SyncSessionDurationsMetricsRecorderTest : public testing::Test {
 public:
  SyncSessionDurationsMetricsRecorderTest()
      : identity_test_env_(&test_url_loader_factory_) {
    // `identity_test_env_` is signed-out by default, whereas `sync_service_` is
    // signed-in by default. Make them consistent.
    sync_service_.SetSignedOut();
  }

  SyncSessionDurationsMetricsRecorderTest(
      const SyncSessionDurationsMetricsRecorderTest&) = delete;
  SyncSessionDurationsMetricsRecorderTest& operator=(
      const SyncSessionDurationsMetricsRecorderTest&) = delete;

  ~SyncSessionDurationsMetricsRecorderTest() override = default;

  void SignIn(signin::ConsentLevel consent_level) {
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        "foo@gmail.com", consent_level);
    sync_service_.SetSignedIn(consent_level, account_info);
    sync_service_.FireStateChanged();
  }

  void SetInvalidCredentialsAuthError() {
    sync_service_.SetPersistentAuthError();

    DCHECK_EQ(sync_service_.GetTransportState(),
              SyncService::TransportState::PAUSED);

    identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_test_env_.identity_manager()->GetPrimaryAccountId(
            signin::ConsentLevel::kSignin),
        GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  }

  void ClearAuthError() {
    identity_test_env_.SetRefreshTokenForPrimaryAccount();
    sync_service_.ClearAuthError();
    ASSERT_EQ(sync_service_.GetTransportState(),
              SyncService::TransportState::ACTIVE);
  }

  std::string GetSessionHistogramName(const std::string& histogram_suffix) {
    return std::string("Session.TotalDurationMax1Day.") + histogram_suffix;
  }

  // TODO(crbug.com/40859574): Deprecate this method.
  std::string GetSessionHistogramLegacyName(
      const std::string& histogram_suffix) {
    return std::string("Session.TotalDuration.") + histogram_suffix;
  }

  void ExpectOneSessionWithDuration(
      const base::HistogramTester& ht,
      const std::vector<std::string>& histogram_suffixes,
      const base::TimeDelta& expected_session_time) {
    for (const std::string& histogram_suffix : histogram_suffixes) {
      ht.ExpectTimeBucketCount(GetSessionHistogramName(histogram_suffix),
                               expected_session_time, 1);
      ht.ExpectTimeBucketCount(GetSessionHistogramLegacyName(histogram_suffix),
                               expected_session_time, 1);
    }
  }

  void ExpectOneSession(const base::HistogramTester& ht,
                        const std::vector<std::string>& histogram_suffixes) {
    for (const std::string& histogram_suffix : histogram_suffixes) {
      ht.ExpectTotalCount(GetSessionHistogramName(histogram_suffix), 1);
      ht.ExpectTotalCount(GetSessionHistogramLegacyName(histogram_suffix), 1);
    }
  }

  void ExpectNoSession(const base::HistogramTester& ht,
                       const std::vector<std::string>& histogram_suffixes) {
    for (const std::string& histogram_suffix : histogram_suffixes) {
      ht.ExpectTotalCount(GetSessionHistogramName(histogram_suffix), 0);
      ht.ExpectTotalCount(GetSessionHistogramLegacyName(histogram_suffix), 0);
    }
  }

  void StartAndEndSession(const base::TimeDelta& session_time) {
    SyncSessionDurationsMetricsRecorder metrics_recorder(
        &sync_service_, identity_test_env_.identity_manager());
    metrics_recorder.OnSessionStarted(base::TimeTicks::Now());
    metrics_recorder.OnSessionEnded(session_time);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  TestSyncService sync_service_;
};

TEST_F(SyncSessionDurationsMetricsRecorderTest, WebSignedOut) {
  base::HistogramTester ht;
  StartAndEndSession(kSessionTime);

  ExpectOneSessionWithDuration(ht, {"WithoutAccount"}, kSessionTime);
  ExpectNoSession(ht, {"WithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest, WebSignedIn) {
  identity_test_env_.SetCookieAccounts({{"foo@gmail.com", "foo_gaia_id"}});

  base::HistogramTester ht;
  StartAndEndSession(kSessionTime);

  ExpectOneSessionWithDuration(ht, {"WithAccount"}, kSessionTime);
  ExpectNoSession(ht, {"WithoutAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest, NotOptedInToSync) {
  base::HistogramTester ht;
  StartAndEndSession(kSessionTime);

  ExpectOneSessionWithDuration(ht, {"NotOptedInToSyncWithoutAccount"},
                               kSessionTime);
  ExpectNoSession(
      ht,
      {"NotOptedInToSyncWithAccount", "NotOptedInToSyncWithAccountInAuthError",
       "OptedInToSyncWithoutAccount", "OptedInToSyncWithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest, OptedInToSync_SyncActive) {
  SignIn(signin::ConsentLevel::kSync);

  base::HistogramTester ht;
  StartAndEndSession(kSessionTime);

  ExpectOneSessionWithDuration(ht, {"OptedInToSyncWithAccount"}, kSessionTime);
  ExpectNoSession(
      ht, {"NotOptedInToSyncWithoutAccount", "NotOptedInToSyncWithoutAccount",
           "NotOptedInToSyncWithAccountInAuthError",
           "OptedInToSyncWithoutAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest,
       OptedInToSync_SyncDisabledByEnterprisePolicy) {
  SignIn(signin::ConsentLevel::kSync);
  sync_service_.SetAllowedByEnterprisePolicy(false);

  base::HistogramTester ht;
  StartAndEndSession(kSessionTime);

  // If the user opted in to sync, but then disabled sync via enterprise policy,
  // then they are counted as having opted out of sync.
  ExpectOneSessionWithDuration(ht, {"NotOptedInToSyncWithAccount"},
                               kSessionTime);
  ExpectNoSession(ht,
                  {"NotOptedInToSyncWithoutAccount",
                   "NotOptedInToSyncWithAccountInAuthError",
                   "OptedInToSyncWithoutAccount", "OptedInToSyncWithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest,
       OptedInToSync_PrimaryAccountInAuthError) {
  SignIn(signin::ConsentLevel::kSync);
  SetInvalidCredentialsAuthError();

  base::HistogramTester ht;
  StartAndEndSession(kSessionTime);

  ExpectOneSessionWithDuration(ht, {"OptedInToSyncWithoutAccount"},
                               kSessionTime);
  ExpectNoSession(
      ht,
      {"NotOptedInToSyncWithoutAccount", "NotOptedInToSyncWithoutAccount",
       "NotOptedInToSyncWithAccountInAuthError", "OptedInToSyncWithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest,
       SyncDisabled_PrimaryAccountInAuthError) {
  SignIn(signin::ConsentLevel::kSignin);
  SetInvalidCredentialsAuthError();

  base::HistogramTester ht;
  StartAndEndSession(kSessionTime);

  ExpectOneSessionWithDuration(ht, {"NotOptedInToSyncWithAccountInAuthError"},
                               kSessionTime);
  ExpectNoSession(
      ht, {"NotOptedInToSyncWithAccount", "NotOptedInToSyncWithoutAccount",
           "OptedInToSyncWithoutAccount", "OptedInToSyncWithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest,
       NotOptedInToSync_SecondaryAccountInAuthError) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("foo@gmail.com");
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  base::HistogramTester ht;
  StartAndEndSession(kSessionTime);

  // The account is in auth error, so they are counted as not having any browser
  // account.
  ExpectOneSessionWithDuration(ht, {"NotOptedInToSyncWithoutAccount"},
                               kSessionTime);
  ExpectNoSession(
      ht,
      {"NotOptedInToSyncWithAccount", "NotOptedInToSyncWithAccountInAuthError",
       "OptedInToSyncWithoutAccount", "OptedInToSyncWithAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest, SyncUnknownOnStartup) {
  SignIn(signin::ConsentLevel::kSync);

  // Simulate sync initializing (before first connection to the server).
  sync_service_.SetLastCycleSnapshot(syncer::SyncCycleSnapshot());
  ASSERT_TRUE(sync_service_.IsSyncFeatureActive());
  ASSERT_FALSE(sync_service_.HasCompletedSyncCycle());

  base::HistogramTester ht;
  StartAndEndSession(kSessionTime);
  ExpectOneSessionWithDuration(ht, {"NotOptedInToSyncWithAccount"},
                               kSessionTime);
  ExpectNoSession(
      ht, {"NotOptedInToSyncWithoutAccount",
           "NotOptedInToSyncWithAccountInAuthError",
           "OptedInToSyncWithoutAccount", "OptedInToSyncWithoutAccount"});
}

TEST_F(SyncSessionDurationsMetricsRecorderTest,
       SyncUnknownOnStartupThenStarts) {
  SignIn(signin::ConsentLevel::kSync);

  // Simulate sync initializing (before first connection to the server).
  SyncCycleSnapshot active_sync_snapshot =
      sync_service_.GetLastCycleSnapshotForDebugging();
  sync_service_.SetLastCycleSnapshot(syncer::SyncCycleSnapshot());
  ASSERT_TRUE(sync_service_.IsSyncFeatureActive());
  ASSERT_FALSE(sync_service_.HasCompletedSyncCycle());

  SyncSessionDurationsMetricsRecorder metrics_recorder(
      &sync_service_, identity_test_env_.identity_manager());

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionStarted(base::TimeTicks::Now());

    // Activate sync
    sync_service_.SetLastCycleSnapshot(active_sync_snapshot);
    ASSERT_TRUE(sync_service_.IsSyncFeatureActive() &&
                sync_service_.HasCompletedSyncCycle());
    metrics_recorder.OnStateChanged(&sync_service_);
    // Sync was in unknown state, so histograms should not be logged.
    ExpectNoSession(
        ht, {"NotOptedInToSyncWithAccount", "NotOptedInToSyncWithoutAccount",
             "NotOptedInToSyncWithAccountInAuthError",
             "OptedInToSyncWithoutAccount", "OptedInToSyncWithoutAccount"});
  }

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionEnded(kSessionTime);
    ExpectOneSession(ht, {"OptedInToSyncWithAccount"});
    ExpectNoSession(
        ht, {"NotOptedInToSyncWithAccount", "NotOptedInToSyncWithoutAccount",
             "NotOptedInToSyncWithAccountInAuthError",
             "OptedInToSyncWithoutAccount"});
  }
}

TEST_F(SyncSessionDurationsMetricsRecorderTest, EnableSync) {
  SyncSessionDurationsMetricsRecorder metrics_recorder(
      &sync_service_, identity_test_env_.identity_manager());

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionStarted(base::TimeTicks::Now());
    SCOPED_TRACE("OnSessionStarted");
    SignIn(signin::ConsentLevel::kSync);

    // The initial state of the record was:
    // 0. sync_status = OFF, signin_status=kSignedOut:
    //    [Current state]: NotOptedInToSyncWithoutAccount.
    //
    // SignIn(signin::ConsentLevel::kSync) does the following:
    // 1. The primary account is set at ConsentLevel::kSync, without a refresh
    //    token:
    //    [Current state]: NotOptedInToSyncWithAccountInAuthError
    //    [Log]: Log previous state: NotOptedInToSyncWithoutAccount
    ExpectOneSession(ht, {"NotOptedInToSyncWithoutAccount"});

    // 2. The refresh token of the primary account is set.
    //    [Current state]: NotOptedInToSyncWithAccount
    //    [Log]: Log previous state: NotOptedInToSyncWithAccountInAuthError
    ExpectOneSession(ht, {"NotOptedInToSyncWithAccountInAuthError"});

    // 3. Sync service is turned on:
    //    [Current state]: OptedInToSyncWithAccount
    //    [Log]: Log previous state: NotOptedInToSyncWithAccount
    ExpectOneSession(ht, {"NotOptedInToSyncWithAccount"});

    ExpectNoSession(
        ht, {"OptedInToSyncWithoutAccount", "OptedInToSyncWithAccount"});
  }

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionEnded(kSessionTime);
    SCOPED_TRACE("OnSessionEnded");
    // 4. When session ends, the last state is logged:
    //    [Log]: Log previous state: OptedInToSyncWithAccount
    ExpectOneSession(ht, {"OptedInToSyncWithAccount"});
    ExpectNoSession(
        ht, {"NotOptedInToSyncWithoutAccount", "NotOptedInToSyncWithAccount",
             "NotOptedInToSyncWithAccountInAuthError",
             "OptedInToSyncWithoutAccount"});
  }
}

TEST_F(SyncSessionDurationsMetricsRecorderTest, EnterAuthError) {
  SignIn(signin::ConsentLevel::kSync);
  SyncSessionDurationsMetricsRecorder metrics_recorder(
      &sync_service_, identity_test_env_.identity_manager());

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionStarted(base::TimeTicks::Now());
    SetInvalidCredentialsAuthError();
    ExpectOneSession(ht, {"OptedInToSyncWithAccount"});
    ExpectNoSession(
        ht, {"NotOptedInToSyncWithAccount", "NotOptedInToSyncWithoutAccount",
             "NotOptedInToSyncWithAccountInAuthError",
             "OptedInToSyncWithoutAccount"});
  }
  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionEnded(kSessionTime);
    ExpectOneSession(ht, {"OptedInToSyncWithoutAccount"});
    ExpectNoSession(
        ht,
        {"NotOptedInToSyncWithAccount", "NotOptedInToSyncWithoutAccount",
         "NotOptedInToSyncWithAccountInAuthError", "OptedInToSyncWithAccount"});
  }
}

TEST_F(SyncSessionDurationsMetricsRecorderTest, FixedAuthError) {
  SignIn(signin::ConsentLevel::kSync);
  SetInvalidCredentialsAuthError();
  SyncSessionDurationsMetricsRecorder metrics_recorder(
      &sync_service_, identity_test_env_.identity_manager());

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionStarted(base::TimeTicks::Now());
    ClearAuthError();
    ExpectOneSession(ht, {"OptedInToSyncWithoutAccount"});
    ExpectNoSession(
        ht,
        {"NotOptedInToSyncWithAccount", "NotOptedInToSyncWithoutAccount",
         "NotOptedInToSyncWithAccountInAuthError", "OptedInToSyncWithAccount"});
  }
  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionEnded(kSessionTime);
    ExpectOneSession(ht, {"OptedInToSyncWithAccount"});
    ExpectNoSession(
        ht, {"NotOptedInToSyncWithAccount", "NotOptedInToSyncWithoutAccount",
             "NotOptedInToSyncWithAccountInAuthError",
             "OptedInToSyncWithoutAccount"});
  }
}

}  // namespace
}  // namespace syncer
