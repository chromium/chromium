// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/activity_reporter/activity_reporter_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "components/activity_reporter/activity_reporter.h"
#include "components/activity_reporter/activity_reporter_for_testing.h"
#include "components/activity_reporter/buildflags.h"
#include "components/activity_reporter/constants.h"
#include "components/activity_reporter/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/cancellation.h"
#include "components/update_client/network.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace activity_reporter {

namespace {

using ::testing::_;

using CrxComponents = std::vector<std::optional<update_client::CrxComponent>>;

// Stands in for the prefs that the real update client writes during an update
// check; the mock writes nothing, so tests dirty this pref to have something
// to commit.
constexpr char kTestPref[] = "test_pref";

// A pref service with the update_client prefs and `kTestPref` registered.
class TestPrefService : public TestingPrefServiceSimple {
 public:
  TestPrefService() {
    update_client::RegisterPrefs(registry());
    registry()->RegisterIntegerPref(kTestPref, 0);
  }
};

// The daynum that tests write as if an update check had succeeded.
constexpr int kTestDaynum = 7100;

constexpr char kPingFreshnessAgeHistogram[] =
    "ActivityReporter.PingFreshnessAge";
constexpr char kPingFreshnessStateHistogram[] =
    "ActivityReporter.PingFreshnessState";

class MockUpdateClient : public update_client::UpdateClient {
 public:
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(base::RepeatingClosure,
              Install,
              (const std::string& id,
               CrxDataCallback crx_data_callback,
               CrxStateChangeCallback crx_state_change_callback,
               update_client::Callback callback),
              (override));
  MOCK_METHOD(void,
              Update,
              (const std::vector<std::string>& ids,
               CrxDataCallback crx_data_callback,
               CrxStateChangeCallback crx_state_change_callback,
               bool is_foreground,
               update_client::Callback callback),
              (override));
  MOCK_METHOD(void,
              CheckForUpdate,
              (const std::string& id,
               CrxDataCallback crx_data_callback,
               CrxStateChangeCallback crx_state_change_callback,
               bool is_foreground,
               update_client::Callback callback),
              (override));
  MOCK_METHOD(bool,
              GetCrxUpdateState,
              (const std::string& id,
               update_client::CrxUpdateItem* update_item),
              (const, override));
  MOCK_METHOD(bool, IsUpdating, (const std::string& id), (const, override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(bool,
              Cancel,
              (const std::string& id, update_client::CancelMode mode),
              (override));
  MOCK_METHOD(void,
              SendPing,
              (const update_client::CrxComponent& crx_component,
               PingParams ping_params,
               update_client::Callback callback),
              (override));
  MOCK_METHOD(void,
              CleanupStaleDownloads,
              (base::Time older_than, base::OnceClosure callback),
              (override));

 private:
  ~MockUpdateClient() override = default;
};

std::unique_ptr<ActivityReporter> CreateReporterForPrefs(
    PrefService* pref_service) {
  return CreateActivityReporterImpl(
      base::BindRepeating([](PrefService* prefs) { return prefs; },
                          pref_service),
      /*network_fetcher_factory=*/nullptr,
      base::BindRepeating([] { return version_info::Channel::UNKNOWN; }),
      base::BindRepeating([] { return std::string("TEST"); }),
      base::DoNothing(), /*per_user_install=*/false);
}

}  // namespace

class ActivityReporterImplTest : public testing::Test {
 protected:
  // Creates a reporter that uses the prefs and update client of this fixture,
  // and reads the time from `clock`.
  std::unique_ptr<ActivityReporter> CreateReporter(const base::Clock* clock) {
    return CreateActivityReporterForTesting(
        base::BindLambdaForTesting(
            [this]() -> PrefService* { return provided_pref_service_; }),
        mock_update_client_,
        base::BindRepeating([] { return version_info::Channel::UNKNOWN; }),
        base::BindRepeating([] { return std::string("TEST"); }),
        base::DoNothing(), clock);
  }

  // Returns the DateLastActive of `app_id` persisted in `pref_service_`.
  int GetDateLastActive(std::string_view app_id) const {
    return persisted_data_->GetDateLastActive(std::string(app_id));
  }

  // Dirties `kTestPref` (with a new value each call, so the pref store has a
  // pending write again even after a previous commit), calls `ReportActive`,
  // and returns the completion callback that the reporter passed to
  // `CheckForUpdate`.
  update_client::Callback ReportActiveWithPendingWrite() {
    pref_service_.SetInteger(kTestPref,
                             pref_service_.GetInteger(kTestPref) + 1);
    EXPECT_FALSE(pref_service_.user_prefs_store()->committed());

    update_client::Callback completion_callback;
    EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _))
        .WillOnce([&](const std::string&,
                      update_client::UpdateClient::CrxDataCallback,
                      update_client::UpdateClient::CrxStateChangeCallback, bool,
                      update_client::Callback callback) {
          completion_callback = std::move(callback);
        });
    activity_reporter_->ReportActive();
    EXPECT_TRUE(completion_callback);
    return completion_callback;
  }

  // Returns the ping freshness that an update check would send now.
  std::string GetPingFreshness() const {
    return persisted_data_->GetPingFreshness(std::string(kChromeActivityId));
  }

  // Writes a new ping freshness, as the update client does when an update
  // check succeeds.
  void WritePingFreshness() {
    persisted_data_->SetDateLastData({std::string(kChromeActivityId)},
                                     kTestDaynum, base::DoNothing());
  }

  // Sets the write time of the ping freshness, or removes it if `write_time` is
  // `std::nullopt`, to emulate a value that an older version wrote, or a clock
  // change.
  void SetPingFreshnessWriteTime(std::optional<base::Time> write_time) {
    update_client::SetPingFreshnessWriteTimeForTesting(
        pref_service_, std::string(kChromeActivityId), write_time);
  }

  // Calls `ReportActive` and returns the ping freshness when the reporter
  // starts the update check. The update check never completes.
  std::string ReportActiveAndGetPingFreshness() {
    std::string ping_freshness;
    EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _))
        .WillOnce([&] { ping_freshness = GetPingFreshness(); });
    activity_reporter_->ReportActive();
    return ping_freshness;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestPrefService pref_service_;
  // What the reporter's pref service provider returns. Tests may clear it to
  // simulate a missing pref service.
  raw_ptr<PrefService> provided_pref_service_ = &pref_service_;
  // Reads and writes the update_client data in `pref_service_`.
  std::unique_ptr<update_client::PersistedData> persisted_data_ =
      update_client::CreatePersistedData(
          base::BindRepeating([](PrefService* prefs) { return prefs; },
                              &pref_service_),
          /*activity_data_service=*/nullptr);
  scoped_refptr<MockUpdateClient> mock_update_client_ =
      base::MakeRefCounted<MockUpdateClient>();
  // The default clock reads the mock time that update_client uses to time the
  // ping freshness. `task_environment_.GetMockClock()` is offset from it.
  std::unique_ptr<ActivityReporter> activity_reporter_ =
      CreateReporter(base::DefaultClock::GetInstance());
};

TEST_F(ActivityReporterImplTest, ReportActiveThrottles) {
  int call_count = 0;
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _))
      .Times(2)
      .WillRepeatedly(
          [&](const std::string& id,
              update_client::UpdateClient::CrxDataCallback crx_data_callback,
              update_client::UpdateClient::CrxStateChangeCallback, bool,
              update_client::Callback) {
            ++call_count;
            EXPECT_EQ(id, kChromeActivityId);
            std::move(crx_data_callback)
                .Run({std::string(kChromeActivityId)}, base::DoNothing());
          });

  // The first report should go through.
  activity_reporter_->ReportActive();
  EXPECT_EQ(call_count, 1);

  // The second report should be throttled.
  activity_reporter_->ReportActive();
  ASSERT_EQ(call_count, 1);

  // Advance the clock by 5 hours. The third report should go through.
  task_environment_.FastForwardBy(base::Hours(5));
  activity_reporter_->ReportActive();
  EXPECT_EQ(call_count, 2);
}

TEST_F(ActivityReporterImplTest, ReportActiveSendsComponentData) {
  base::test::TestFuture<const CrxComponents&> components_future;
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _))
      .WillOnce(
          [&](const std::string& id,
              update_client::UpdateClient::CrxDataCallback crx_data_callback,
              update_client::UpdateClient::CrxStateChangeCallback, bool,
              update_client::Callback) {
            EXPECT_EQ(id, kChromeActivityId);
            std::move(crx_data_callback)
                .Run({std::string(kChromeActivityId)},
                     components_future.GetCallback());
          });
  activity_reporter_->ReportActive();

  ASSERT_TRUE(components_future.IsReady());
  const CrxComponents& components = components_future.Get();
  ASSERT_EQ(components.size(), 1u);
  ASSERT_TRUE(components[0].has_value());
  EXPECT_EQ(components[0]->app_id, kChromeActivityId);
  ASSERT_TRUE(components[0]->version.IsValid());
  EXPECT_EQ(components[0]->version, version_info::GetVersion());
  EXPECT_EQ(components[0]->brand, "TEST");
  EXPECT_EQ(components[0]->channel, "unknown");
  EXPECT_FALSE(components[0]->updates_enabled);
}

TEST_F(ActivityReporterImplTest, ReportActiveMarksFirstReport) {
  // The reporter must write the first-time marker before the update check
  // sends it.
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _))
      .WillOnce([this] {
        EXPECT_EQ(GetDateLastActive(kChromeActivityId),
                  update_client::kDateFirstTime);
      });

  activity_reporter_->ReportActive();
}

TEST_F(ActivityReporterImplTest, ReportActiveKeepsPersistedDateLastActive) {
  persisted_data_->SetDateLastActive(std::string(kChromeActivityId), 7000);
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _));

  activity_reporter_->ReportActive();
  EXPECT_EQ(GetDateLastActive(kChromeActivityId), 7000);
}

TEST_F(ActivityReporterImplTest, ReportActiveNoMarkerWithoutPrefService) {
  // The pref service provider may return null when the first report starts.
  // The reporter must then still send it, without the first-time marker.
  provided_pref_service_ = nullptr;
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _));

  base::HistogramTester histogram_tester;
  activity_reporter_->ReportActive();
  EXPECT_EQ(GetDateLastActive(kChromeActivityId), update_client::kDateUnknown);
  // Without prefs, the persisted state reads as unknown.
  histogram_tester.ExpectUniqueSample(
      "ActivityReporter.InitialDateLastActiveState",
      InitialDateLastActiveState::kUnknown, 1);
}

TEST_F(ActivityReporterImplTest, ConstructionRecordsNoMetricsAndWritesNoPrefs) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<ActivityReporter> reporter =
      CreateReporterForPrefs(&pref_service_);
  histogram_tester.ExpectTotalCount(
      "ActivityReporter.InitialDateLastActiveState", 0);
  EXPECT_FALSE(
      pref_service_.HasPrefPath(update_client::kPersistedDataPreference));
}

TEST_F(ActivityReporterImplTest, ReportActiveCommitsPendingWrite) {
  update_client::Callback completion_callback = ReportActiveWithPendingWrite();

  // The commit must wait for the update check to complete.
  EXPECT_FALSE(pref_service_.user_prefs_store()->committed());
  std::move(completion_callback).Run(update_client::Error::NONE);
  EXPECT_TRUE(pref_service_.user_prefs_store()->committed());
}

TEST_F(ActivityReporterImplTest, ReportActiveCommitsPendingWriteOnError) {
  update_client::Callback completion_callback = ReportActiveWithPendingWrite();

  // Prefs written by a failed update check are committed as well.
  std::move(completion_callback).Run(update_client::Error::UPDATE_CHECK_ERROR);
  EXPECT_TRUE(pref_service_.user_prefs_store()->committed());
}

TEST_F(ActivityReporterImplTest, ReportActiveNoCommitAfterDestruction) {
  update_client::Callback completion_callback = ReportActiveWithPendingWrite();

  // The completion callback may run after the reporter is gone, e.g. during
  // shutdown. It must not touch the pref service then.
  activity_reporter_.reset();
  std::move(completion_callback).Run(update_client::Error::NONE);
  EXPECT_FALSE(pref_service_.user_prefs_store()->committed());
}

TEST_F(ActivityReporterImplTest, ReportActiveNoCommitWithoutPrefService) {
  update_client::Callback completion_callback = ReportActiveWithPendingWrite();

  // The pref service provider may return null by the time the update check
  // completes. The reporter must then skip the commit rather than crash.
  provided_pref_service_ = nullptr;
  std::move(completion_callback).Run(update_client::Error::NONE);
  EXPECT_FALSE(pref_service_.user_prefs_store()->committed());
}

TEST_F(ActivityReporterImplTest, ReportActiveRecordsPingFreshnessStateNone) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ReportActiveAndGetPingFreshness().empty());
  histogram_tester.ExpectUniqueSample(kPingFreshnessStateHistogram,
                                      update_client::PingFreshnessState::kNone,
                                      1);
  histogram_tester.ExpectTotalCount(kPingFreshnessAgeHistogram, 0);
}

TEST_F(ActivityReporterImplTest, ReportActiveKeepsFreshPingFreshness) {
  base::test::ScopedFeatureList feature_list(
      features::kActivityReporterPingFreshnessRotation);
  base::HistogramTester histogram_tester;
  WritePingFreshness();
  const std::string ping_freshness = GetPingFreshness();
  task_environment_.FastForwardBy(update_client::kPingFreshnessMaxAge -
                                  base::Seconds(1));
  EXPECT_EQ(ReportActiveAndGetPingFreshness(), ping_freshness);
  EXPECT_FALSE(pref_service_.user_prefs_store()->committed());
  histogram_tester.ExpectUniqueSample(kPingFreshnessStateHistogram,
                                      update_client::PingFreshnessState::kFresh,
                                      1);
  histogram_tester.ExpectUniqueSample(
      kPingFreshnessAgeHistogram,
      update_client::kPingFreshnessMaxAge.InHours() - 1, 1);
}

TEST_F(ActivityReporterImplTest, ReportActiveRotatesExpiredPingFreshness) {
  base::test::ScopedFeatureList feature_list(
      features::kActivityReporterPingFreshnessRotation);
  base::HistogramTester histogram_tester;
  WritePingFreshness();
  const std::string ping_freshness = GetPingFreshness();
  task_environment_.FastForwardBy(update_client::kPingFreshnessMaxAge);

  // The reporter replaces the value, and commits it, before the update check.
  const std::string sent_ping_freshness = ReportActiveAndGetPingFreshness();
  EXPECT_FALSE(sent_ping_freshness.empty());
  EXPECT_NE(sent_ping_freshness, ping_freshness);
  EXPECT_TRUE(pref_service_.user_prefs_store()->committed());
  histogram_tester.ExpectUniqueSample(
      kPingFreshnessStateHistogram, update_client::PingFreshnessState::kExpired,
      1);
  histogram_tester.ExpectUniqueSample(
      kPingFreshnessAgeHistogram, update_client::kPingFreshnessMaxAge.InHours(),
      1);
}

TEST_F(ActivityReporterImplTest, ReportActiveKeepsRotatedPingFreshness) {
  base::test::ScopedFeatureList feature_list(
      features::kActivityReporterPingFreshnessRotation);
  WritePingFreshness();
  task_environment_.FastForwardBy(update_client::kPingFreshnessMaxAge);
  const std::string rotated_ping_freshness = ReportActiveAndGetPingFreshness();

  // The next report sends the value that the previous report rotated in.
  base::HistogramTester histogram_tester;
  task_environment_.FastForwardBy(base::Hours(5));
  EXPECT_EQ(ReportActiveAndGetPingFreshness(), rotated_ping_freshness);
  histogram_tester.ExpectUniqueSample(kPingFreshnessStateHistogram,
                                      update_client::PingFreshnessState::kFresh,
                                      1);
  histogram_tester.ExpectUniqueSample(kPingFreshnessAgeHistogram, 5, 1);
}

TEST_F(ActivityReporterImplTest,
       ReportActiveRotatesPingFreshnessWithUnknownWriteTime) {
  base::test::ScopedFeatureList feature_list(
      features::kActivityReporterPingFreshnessRotation);
  base::HistogramTester histogram_tester;
  WritePingFreshness();
  const std::string ping_freshness = GetPingFreshness();
  SetPingFreshnessWriteTime(std::nullopt);
  EXPECT_NE(ReportActiveAndGetPingFreshness(), ping_freshness);
  EXPECT_TRUE(pref_service_.user_prefs_store()->committed());
  histogram_tester.ExpectUniqueSample(
      kPingFreshnessStateHistogram,
      update_client::PingFreshnessState::kUnknownWriteTime, 1);
  histogram_tester.ExpectTotalCount(kPingFreshnessAgeHistogram, 0);
}

TEST_F(ActivityReporterImplTest,
       ReportActiveRotatesPingFreshnessWrittenInTheFuture) {
  base::test::ScopedFeatureList feature_list(
      features::kActivityReporterPingFreshnessRotation);
  base::HistogramTester histogram_tester;
  WritePingFreshness();
  const std::string ping_freshness = GetPingFreshness();
  SetPingFreshnessWriteTime(base::Time::Now() + base::Minutes(1));
  EXPECT_NE(ReportActiveAndGetPingFreshness(), ping_freshness);
  EXPECT_TRUE(pref_service_.user_prefs_store()->committed());
  histogram_tester.ExpectUniqueSample(
      kPingFreshnessStateHistogram,
      update_client::PingFreshnessState::kFutureWriteTime, 1);
  histogram_tester.ExpectTotalCount(kPingFreshnessAgeHistogram, 0);
}

TEST_F(ActivityReporterImplTest,
       ReportActiveKeepsExpiredPingFreshnessIfRotationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kActivityReporterPingFreshnessRotation);
  base::HistogramTester histogram_tester;
  WritePingFreshness();
  const std::string ping_freshness = GetPingFreshness();
  task_environment_.FastForwardBy(update_client::kPingFreshnessMaxAge);
  EXPECT_EQ(ReportActiveAndGetPingFreshness(), ping_freshness);
  EXPECT_FALSE(pref_service_.user_prefs_store()->committed());
  histogram_tester.ExpectUniqueSample(
      kPingFreshnessStateHistogram, update_client::PingFreshnessState::kExpired,
      1);
  histogram_tester.ExpectUniqueSample(
      kPingFreshnessAgeHistogram, update_client::kPingFreshnessMaxAge.InHours(),
      1);
}

TEST_F(ActivityReporterImplTest,
       ReportActiveKeepsPingFreshnessYoungerThanMaxAgeParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kActivityReporterPingFreshnessRotation,
      {{features::kPingFreshnessRotationMaxAge.name, "72h"}});
  base::HistogramTester histogram_tester;
  WritePingFreshness();
  const std::string ping_freshness = GetPingFreshness();
  task_environment_.FastForwardBy(base::Hours(72) - base::Seconds(1));
  EXPECT_EQ(ReportActiveAndGetPingFreshness(), ping_freshness);
  histogram_tester.ExpectUniqueSample(kPingFreshnessStateHistogram,
                                      update_client::PingFreshnessState::kFresh,
                                      1);
}

TEST_F(ActivityReporterImplTest,
       ReportActiveRotatesPingFreshnessAtMaxAgeParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kActivityReporterPingFreshnessRotation,
      {{features::kPingFreshnessRotationMaxAge.name, "72h"}});
  base::HistogramTester histogram_tester;
  WritePingFreshness();
  const std::string ping_freshness = GetPingFreshness();
  task_environment_.FastForwardBy(base::Hours(72));
  EXPECT_NE(ReportActiveAndGetPingFreshness(), ping_freshness);
  histogram_tester.ExpectUniqueSample(
      kPingFreshnessStateHistogram, update_client::PingFreshnessState::kExpired,
      1);
}

TEST(ActivityReporterFeaturesTest, PingFreshnessRotationMaxAgeWithoutParam) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kActivityReporterPingFreshnessRotation);
    EXPECT_EQ(features::GetPingFreshnessRotationMaxAge(), base::Hours(53));
  }
  base::test::ScopedFeatureList feature_list(
      features::kActivityReporterPingFreshnessRotation);
  EXPECT_EQ(features::GetPingFreshnessRotationMaxAge(), base::Hours(53));
}

TEST(ActivityReporterFeaturesTest, PingFreshnessRotationMaxAgeParam) {
  constexpr struct {
    const char* param;
    base::TimeDelta max_age;
  } kCases[] = {
      {"72h", base::Hours(72)},
      // The max age is clamped to between 24 hours and 28 days.
      {"24h", base::Hours(24)},
      {"23h", base::Hours(24)},
      {"28d", base::Days(28)},
      {"29d", base::Days(28)},
      // A value that isn't positive falls back to the default.
      {"0", base::Hours(53)},
      {"-1h", base::Hours(53)},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.param);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kActivityReporterPingFreshnessRotation,
        {{features::kPingFreshnessRotationMaxAge.name, test_case.param}});
    EXPECT_EQ(features::GetPingFreshnessRotationMaxAge(), test_case.max_age);
  }
}

TEST_F(ActivityReporterImplTest,
       ThrottledReportActiveRecordsNoPingFreshnessMetrics) {
  WritePingFreshness();
  ReportActiveAndGetPingFreshness();
  testing::Mock::VerifyAndClearExpectations(mock_update_client_.get());

  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _)).Times(0);
  task_environment_.FastForwardBy(base::Hours(1));
  activity_reporter_->ReportActive();
  histogram_tester.ExpectTotalCount(kPingFreshnessStateHistogram, 0);
  histogram_tester.ExpectTotalCount(kPingFreshnessAgeHistogram, 0);
}

TEST_F(ActivityReporterImplTest, IncompleteReportsNoReports) {
  base::HistogramTester histogram_tester;
  activity_reporter_.reset();
  histogram_tester.ExpectUniqueSample("ActivityReporter.IncompleteReports", 0,
                                      1);
}

TEST_F(ActivityReporterImplTest, IncompleteReportsCompleted) {
  base::HistogramTester histogram_tester;
  update_client::Callback completion_callback = ReportActiveWithPendingWrite();
  std::move(completion_callback).Run(update_client::Error::NONE);
  activity_reporter_.reset();
  histogram_tester.ExpectUniqueSample("ActivityReporter.IncompleteReports", 0,
                                      1);
}

TEST_F(ActivityReporterImplTest, IncompleteReportsCompletedWithError) {
  base::HistogramTester histogram_tester;
  update_client::Callback completion_callback = ReportActiveWithPendingWrite();
  std::move(completion_callback).Run(update_client::Error::UPDATE_CHECK_ERROR);
  activity_reporter_.reset();
  histogram_tester.ExpectUniqueSample("ActivityReporter.IncompleteReports", 0,
                                      1);
}

TEST_F(ActivityReporterImplTest, IncompleteReportsOneInFlight) {
  base::HistogramTester histogram_tester;
  update_client::Callback completion_callback = ReportActiveWithPendingWrite();
  activity_reporter_.reset();
  histogram_tester.ExpectUniqueSample("ActivityReporter.IncompleteReports", 1,
                                      1);

  // Completing the report after destruction does not emit again.
  std::move(completion_callback).Run(update_client::Error::NONE);
  histogram_tester.ExpectTotalCount("ActivityReporter.IncompleteReports", 1);
}

TEST_F(ActivityReporterImplTest, IncompleteReportsThrottledNotCounted) {
  base::HistogramTester histogram_tester;
  ReportActiveWithPendingWrite();

  // A report within the throttling interval does not start an update check, so
  // it is not counted as incomplete.
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _)).Times(0);
  task_environment_.FastForwardBy(base::Hours(1));
  activity_reporter_->ReportActive();
  activity_reporter_.reset();
  histogram_tester.ExpectUniqueSample("ActivityReporter.IncompleteReports", 1,
                                      1);
}

TEST_F(ActivityReporterImplTest, IncompleteReportsMultipleInFlight) {
  base::HistogramTester histogram_tester;
  ReportActiveWithPendingWrite();
  task_environment_.FastForwardBy(base::Hours(5));
  update_client::Callback second_callback = ReportActiveWithPendingWrite();
  task_environment_.FastForwardBy(base::Hours(5));
  ReportActiveWithPendingWrite();

  // Only the second report completes.
  std::move(second_callback).Run(update_client::Error::NONE);
  activity_reporter_.reset();
  histogram_tester.ExpectUniqueSample("ActivityReporter.IncompleteReports", 2,
                                      1);
}

TEST_F(ActivityReporterImplTest, InitialDateLastActiveStateUnknown) {
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _));

  base::HistogramTester histogram_tester;
  activity_reporter_->ReportActive();
  histogram_tester.ExpectUniqueSample(
      "ActivityReporter.InitialDateLastActiveState",
      InitialDateLastActiveState::kUnknown, 1);
}

TEST_F(ActivityReporterImplTest,
       InitialDateLastActiveStateFirstTimeUnpersisted) {
  persisted_data_->SetDateLastActive(std::string(kChromeActivityId),
                                     update_client::kDateFirstTime);
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _));

  base::HistogramTester histogram_tester;
  activity_reporter_->ReportActive();
  histogram_tester.ExpectUniqueSample(
      "ActivityReporter.InitialDateLastActiveState",
      InitialDateLastActiveState::kFirstTimeUnpersisted, 1);
}

TEST_F(ActivityReporterImplTest, InitialDateLastActiveStatePersisted) {
  persisted_data_->SetDateLastActive(std::string(kChromeActivityId), 7100);
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _));

  base::HistogramTester histogram_tester;
  activity_reporter_->ReportActive();
  histogram_tester.ExpectUniqueSample(
      "ActivityReporter.InitialDateLastActiveState",
      InitialDateLastActiveState::kPersisted, 1);
}

TEST_F(ActivityReporterImplTest, InitialDateLastActiveStateInvalid) {
  persisted_data_->SetDateLastActive(std::string(kChromeActivityId), -3);
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _));

  base::HistogramTester histogram_tester;
  activity_reporter_->ReportActive();
  histogram_tester.ExpectUniqueSample(
      "ActivityReporter.InitialDateLastActiveState",
      InitialDateLastActiveState::kInvalid, 1);
}

TEST_F(ActivityReporterImplTest, InitialDateLastActiveStateRecordedOnce) {
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _)).Times(2);

  base::HistogramTester histogram_tester;
  activity_reporter_->ReportActive();
  histogram_tester.ExpectUniqueSample(
      "ActivityReporter.InitialDateLastActiveState",
      InitialDateLastActiveState::kUnknown, 1);

  // Later reports, throttled or not, don't record it again.
  activity_reporter_->ReportActive();
  task_environment_.FastForwardBy(base::Hours(5));
  activity_reporter_->ReportActive();
  histogram_tester.ExpectUniqueSample(
      "ActivityReporter.InitialDateLastActiveState",
      InitialDateLastActiveState::kUnknown, 1);
}

TEST_F(ActivityReporterImplTest, ReportActiveIgnoresLastReportInFuture) {
  // The task environment's mock clock can't move back, so this test uses its
  // own clock and reporter.
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  std::unique_ptr<ActivityReporter> reporter = CreateReporter(&clock);
  int call_count = 0;
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _))
      .WillRepeatedly([&] { ++call_count; });

  reporter->ReportActive();
  EXPECT_EQ(call_count, 1);

  // Move the clock back a day, which puts the last report in the future.
  clock.Advance(-base::Days(1));
  reporter->ReportActive();
  EXPECT_EQ(call_count, 2);

  // The throttle restarts from the new time: it drops reports for 5 hours,
  // then lets the next one through.
  reporter->ReportActive();
  EXPECT_EQ(call_count, 2);
  clock.Advance(base::Hours(5) - base::Seconds(1));
  reporter->ReportActive();
  EXPECT_EQ(call_count, 2);
  clock.Advance(base::Seconds(1));
  reporter->ReportActive();
  EXPECT_EQ(call_count, 3);
}

TEST_F(ActivityReporterImplTest, ReportActiveThrottlesAfterSmallStepBack) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  std::unique_ptr<ActivityReporter> reporter = CreateReporter(&clock);
  int call_count = 0;
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _))
      .WillRepeatedly([&] { ++call_count; });

  reporter->ReportActive();
  EXPECT_EQ(call_count, 1);
  clock.Advance(base::Hours(1));
  reporter->ReportActive();
  EXPECT_EQ(call_count, 1);

  // Move the clock back 30 minutes. The last report is still in the past, so
  // the throttle holds until the clock reads 5 hours after it.
  clock.Advance(-base::Minutes(30));
  reporter->ReportActive();
  EXPECT_EQ(call_count, 1);
  clock.Advance(base::Hours(4) + base::Minutes(29));
  reporter->ReportActive();
  EXPECT_EQ(call_count, 1);
  clock.Advance(base::Minutes(1));
  reporter->ReportActive();
  EXPECT_EQ(call_count, 2);
}

TEST(ActivityReporterFactoryTest, CreatesRealReporterOnlyWithReportActivity) {
  base::test::TaskEnvironment task_environment;
  TestingPrefServiceSimple pref_service;
  update_client::RegisterPrefs(pref_service.registry());

  base::HistogramTester histogram_tester;
  std::unique_ptr<ActivityReporter> reporter = CreateActivityReporter(
      base::BindRepeating([](PrefService* prefs) { return prefs; },
                          &pref_service),
      /*network_fetcher_factory=*/nullptr,
      base::BindRepeating([] { return version_info::Channel::UNKNOWN; }),
      base::BindRepeating([] { return std::string("TEST"); }),
      base::DoNothing(), /*per_user_install=*/false);
  ASSERT_TRUE(reporter);
  reporter.reset();
  // Only the real reporter records `ActivityReporter.IncompleteReports` when
  // destroyed.
#if BUILDFLAG(REPORT_ACTIVITY)
  histogram_tester.ExpectUniqueSample("ActivityReporter.IncompleteReports", 0,
                                      1);
#else
  histogram_tester.ExpectTotalCount("ActivityReporter.IncompleteReports", 0);
#endif
}

}  // namespace activity_reporter
