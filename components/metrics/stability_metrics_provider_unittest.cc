// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_provider.h"

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

class StabilityMetricsProviderTest : public testing::Test {
 public:
  StabilityMetricsProviderTest() {
    StabilityMetricsProvider::RegisterPrefs(prefs_.registry());
  }

  StabilityMetricsProviderTest(const StabilityMetricsProviderTest&) = delete;
  StabilityMetricsProviderTest& operator=(const StabilityMetricsProviderTest&) =
      delete;

  ~StabilityMetricsProviderTest() override = default;

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(StabilityMetricsProviderTest, ProvideStabilityMetrics) {
  base::HistogramTester histogram_tester;
  StabilityMetricsProvider stability_provider(&prefs_);
  MetricsProvider* provider = &stability_provider;
  SystemProfileProto system_profile;
  provider->ProvideStabilityMetrics(&system_profile);

#if BUILDFLAG(IS_ANDROID)
  // Initial log metrics: only expected if non-zero.
  const SystemProfileProto_Stability& stability = system_profile.stability();
  // The launch count field is used on Android only.
  EXPECT_FALSE(stability.has_launch_count());
#endif

  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kLaunch, 0);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kBrowserCrash, 0);
}

TEST_F(StabilityMetricsProviderTest, RecordStabilityMetrics) {
  base::HistogramTester histogram_tester;
  {
    StabilityMetricsProvider recorder(&prefs_);
    recorder.LogLaunch();
    recorder.LogCrash(base::Time());
  }

  {
    StabilityMetricsProvider stability_provider(&prefs_);
    MetricsProvider* provider = &stability_provider;
    SystemProfileProto system_profile;
    provider->ProvideStabilityMetrics(&system_profile);

#if BUILDFLAG(IS_ANDROID)
    // Initial log metrics: only expected if non-zero.
    const SystemProfileProto_Stability& stability = system_profile.stability();
    // The launch count field is populated only on Android.
    EXPECT_EQ(1, stability.launch_count());
#endif

    histogram_tester.ExpectBucketCount("Stability.Counts2",
                                       StabilityEventType::kLaunch, 1);
    histogram_tester.ExpectBucketCount("Stability.Counts2",
                                       StabilityEventType::kBrowserCrash, 1);
  }
}

#if BUILDFLAG(IS_WIN)
namespace {

void WaitForSystemSessionChecks(base::test::TaskEnvironment& task_environment) {
  // Workers post their replies before the flush completes. Queue the quit
  // behind those replies, rather than quitting directly on a worker.
  base::ThreadPoolInstance::Get()->FlushAsyncForTesting(
      base::BindPostTask(task_environment.GetMainThreadTaskRunner(),
                         task_environment.QuitClosure()));
  task_environment.RunUntilQuit();
}

}  // namespace

TEST_F(StabilityMetricsProviderTest, RecordSystemCrashMetrics) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  {
    const base::Time unclean_time = base::Time::Now();
    StabilityMetricsProvider recorder(&prefs_);

    // Stub out the System event-log traversal: treat a crash whose
    // last_live_timestamp equals `unclean_time` as an unclean system session.
    recorder.SetIsUncleanSystemSessionCallbackForTesting(base::BindRepeating(
        [](base::Time unclean_time, base::Time last_live_timestamp) {
          return last_live_timestamp == unclean_time;
        },
        unclean_time));

    // Logs a system crash.
    recorder.LogCrash(unclean_time);

    // Logs a crash that is not a system crash.
    recorder.LogCrash(unclean_time - base::Minutes(1));

    // Wait for both thread-pool checks and their replies on this sequence.
    WaitForSystemSessionChecks(task_environment);
    EXPECT_EQ(1, prefs_.GetInteger(prefs::kStabilitySystemCrashCount));
  }

  {
    StabilityMetricsProvider stability_provider(&prefs_);
    MetricsProvider* provider = &stability_provider;
    SystemProfileProto system_profile;
    provider->ProvideStabilityMetrics(&system_profile);

    // Two crashes, one system crash.
    histogram_tester.ExpectUniqueSample("Stability.Counts2",
                                        StabilityEventType::kBrowserCrash, 2);
    histogram_tester.ExpectTotalCount("Stability.Internals.SystemCrashCount",
                                      1);
  }
}

TEST_F(StabilityMetricsProviderTest, DropsSystemCrashAfterProviderDestruction) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  {
    StabilityMetricsProvider recorder(&prefs_);
    recorder.SetIsUncleanSystemSessionCallbackForTesting(
        base::BindRepeating([](base::Time) { return true; }));
    recorder.LogCrash(base::Time::Now());
  }

  WaitForSystemSessionChecks(task_environment);
  EXPECT_EQ(0, prefs_.GetInteger(prefs::kStabilitySystemCrashCount));
}

TEST_F(StabilityMetricsProviderTest, DropsSystemCrashWhenMetricsAreCleared) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  StabilityMetricsProvider recorder(&prefs_);
  recorder.SetIsUncleanSystemSessionCallbackForTesting(
      base::BindRepeating([](base::Time) { return true; }));
  recorder.LogCrash(base::Time::Now());

  MetricsProvider* provider = &recorder;
  provider->ClearSavedStabilityMetrics();
  WaitForSystemSessionChecks(task_environment);
  EXPECT_EQ(0, prefs_.GetInteger(prefs::kStabilitySystemCrashCount));

  recorder.LogCrash(base::Time::Now());
  WaitForSystemSessionChecks(task_environment);
  EXPECT_EQ(1, prefs_.GetInteger(prefs::kStabilitySystemCrashCount));
}

#endif

}  // namespace metrics
