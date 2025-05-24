// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/history_sync_session_durations_metrics_recorder.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

constexpr base::TimeDelta kSessionTime = base::Seconds(10);

constexpr char kMetricNameWithoutHistorySync[] =
    "Session.TotalDurationMax1Day.WithoutHistorySync";
constexpr char kMetricNameWithHistorySync[] =
    "Session.TotalDurationMax1Day.WithHistorySync";
constexpr char kMetricNameWithHistorySyncWithoutAuthError[] =
    "Session.TotalDurationMax1Day.WithHistorySyncWithoutAuthError";
constexpr char kMetricNameWithHistorySyncAndAuthError[] =
    "Session.TotalDurationMax1Day.WithHistorySyncAndAuthError";

class HistorySyncSessionDurationsMetricsRecorderTest : public testing::Test {
 public:
  HistorySyncSessionDurationsMetricsRecorderTest() = default;
  ~HistorySyncSessionDurationsMetricsRecorderTest() override = default;

  void SignIn(bool history_sync_enabled) {
    sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
    sync_service_.GetUserSettings()->SetSelectedType(
        UserSelectableType::kHistory, history_sync_enabled);
    sync_service_.FireStateChanged();
  }

 protected:
  TestSyncService sync_service_;
};

TEST_F(HistorySyncSessionDurationsMetricsRecorderTest, InitiallySignedOut) {
  sync_service_.SetSignedOut();

  base::HistogramTester ht;
  HistorySyncSessionDurationsMetricsRecorder metrics_recorder(&sync_service_);
  metrics_recorder.OnSessionStarted();
  metrics_recorder.OnSessionEnded(kSessionTime);

  ht.ExpectUniqueTimeSample(kMetricNameWithoutHistorySync, kSessionTime, 1);
  ht.ExpectTotalCount(kMetricNameWithHistorySync, 0);
  ht.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 0);
  ht.ExpectTotalCount(kMetricNameWithHistorySyncAndAuthError, 0);
}

TEST_F(HistorySyncSessionDurationsMetricsRecorderTest,
       InitiallySignedInWithoutHistorySync) {
  SignIn(/*history_sync_enabled=*/false);

  base::HistogramTester ht;
  HistorySyncSessionDurationsMetricsRecorder metrics_recorder(&sync_service_);
  metrics_recorder.OnSessionStarted();
  metrics_recorder.OnSessionEnded(kSessionTime);

  ht.ExpectUniqueTimeSample(kMetricNameWithoutHistorySync, kSessionTime, 1);
  ht.ExpectTotalCount(kMetricNameWithHistorySync, 0);
  ht.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 0);
  ht.ExpectTotalCount(kMetricNameWithHistorySyncAndAuthError, 0);
}

TEST_F(HistorySyncSessionDurationsMetricsRecorderTest,
       InitiallySignedInWithHistorySync) {
  SignIn(/*history_sync_enabled=*/true);

  base::HistogramTester ht;
  HistorySyncSessionDurationsMetricsRecorder metrics_recorder(&sync_service_);
  metrics_recorder.OnSessionStarted();
  metrics_recorder.OnSessionEnded(kSessionTime);

  ht.ExpectUniqueTimeSample(kMetricNameWithHistorySync, kSessionTime, 1);
  ht.ExpectUniqueTimeSample(kMetricNameWithHistorySyncWithoutAuthError,
                            kSessionTime, 1);
  ht.ExpectTotalCount(kMetricNameWithoutHistorySync, 0);
  ht.ExpectTotalCount(kMetricNameWithHistorySyncAndAuthError, 0);
}

TEST_F(HistorySyncSessionDurationsMetricsRecorderTest,
       InitiallySignedInWithHistorySyncAndAuthError) {
  SignIn(/*history_sync_enabled=*/true);
  sync_service_.SetPersistentAuthError();

  base::HistogramTester ht;
  HistorySyncSessionDurationsMetricsRecorder metrics_recorder(&sync_service_);
  metrics_recorder.OnSessionStarted();
  metrics_recorder.OnSessionEnded(kSessionTime);

  ht.ExpectUniqueTimeSample(kMetricNameWithHistorySync, kSessionTime, 1);
  ht.ExpectUniqueTimeSample(kMetricNameWithHistorySyncAndAuthError,
                            kSessionTime, 1);
  ht.ExpectTotalCount(kMetricNameWithoutHistorySync, 0);
  ht.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 0);
}

TEST_F(HistorySyncSessionDurationsMetricsRecorderTest,
       SignInAndEnableHistorySync) {
  sync_service_.SetSignedOut();
  HistorySyncSessionDurationsMetricsRecorder metrics_recorder(&sync_service_);

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionStarted();
    SCOPED_TRACE("OnSessionStarted");

    SignIn(/*history_sync_enabled=*/true);

    ht.ExpectTotalCount(kMetricNameWithoutHistorySync, 1);
    ht.ExpectTotalCount(kMetricNameWithHistorySync, 0);
    ht.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 0);
    ht.ExpectTotalCount(kMetricNameWithHistorySyncAndAuthError, 0);
  }

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionEnded(kSessionTime);
    SCOPED_TRACE("OnSessionEnded");

    ht.ExpectUniqueTimeSample(kMetricNameWithHistorySync, kSessionTime, 1);
    ht.ExpectUniqueTimeSample(kMetricNameWithHistorySyncWithoutAuthError,
                              kSessionTime, 1);
    ht.ExpectTotalCount(kMetricNameWithoutHistorySync, 0);
    ht.ExpectTotalCount(kMetricNameWithHistorySyncAndAuthError, 0);
  }
}

TEST_F(HistorySyncSessionDurationsMetricsRecorderTest, EnterAuthError) {
  SignIn(/*history_sync_enabled=*/true);
  HistorySyncSessionDurationsMetricsRecorder metrics_recorder(&sync_service_);

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionStarted();
    sync_service_.SetPersistentAuthError();
    sync_service_.FireStateChanged();

    ht.ExpectTotalCount(kMetricNameWithHistorySync, 1);
    ht.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 1);
    ht.ExpectTotalCount(kMetricNameWithoutHistorySync, 0);
    ht.ExpectTotalCount(kMetricNameWithHistorySyncAndAuthError, 0);
  }

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionEnded(kSessionTime);

    ht.ExpectUniqueTimeSample(kMetricNameWithHistorySync, kSessionTime, 1);
    ht.ExpectUniqueTimeSample(kMetricNameWithHistorySyncAndAuthError,
                              kSessionTime, 1);
    ht.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 0);
    ht.ExpectTotalCount(kMetricNameWithoutHistorySync, 0);
  }
}

TEST_F(HistorySyncSessionDurationsMetricsRecorderTest, FixAuthError) {
  SignIn(/*history_sync_enabled=*/true);
  sync_service_.SetPersistentAuthError();
  HistorySyncSessionDurationsMetricsRecorder metrics_recorder(&sync_service_);

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionStarted();
    sync_service_.ClearAuthError();
    sync_service_.FireStateChanged();

    ht.ExpectTotalCount(kMetricNameWithHistorySync, 1);
    ht.ExpectTotalCount(kMetricNameWithHistorySyncAndAuthError, 1);
    ht.ExpectTotalCount(kMetricNameWithHistorySyncWithoutAuthError, 0);
    ht.ExpectTotalCount(kMetricNameWithoutHistorySync, 0);
  }

  {
    base::HistogramTester ht;
    metrics_recorder.OnSessionEnded(kSessionTime);

    ht.ExpectUniqueTimeSample(kMetricNameWithHistorySync, kSessionTime, 1);
    ht.ExpectUniqueTimeSample(kMetricNameWithHistorySyncWithoutAuthError,
                              kSessionTime, 1);
    ht.ExpectTotalCount(kMetricNameWithoutHistorySync, 0);
    ht.ExpectTotalCount(kMetricNameWithHistorySyncAndAuthError, 0);
  }
}

}  // namespace
}  // namespace syncer
