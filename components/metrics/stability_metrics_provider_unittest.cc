// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
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

class TestingStabilityMetricsProvider : public StabilityMetricsProvider {
 public:
  TestingStabilityMetricsProvider(PrefService* local_state,
                                  base::Time unclean_session_time)
      : StabilityMetricsProvider(local_state),
        unclean_session_time_(unclean_session_time) {}

  bool IsUncleanSystemSession(base::Time last_live_timestamp) override {
    return last_live_timestamp == unclean_session_time_;
  }

 private:
  const base::Time unclean_session_time_;
};

}  // namespace

TEST_F(StabilityMetricsProviderTest, RecordSystemCrashMetrics) {
  base::HistogramTester histogram_tester;
  {
    base::Time unclean_time = base::Time::Now();
    TestingStabilityMetricsProvider recorder(&prefs_, unclean_time);

    // Any crash with a last_live_timestamp equal to unclean_time will
    // be logged as a system crash as per the implementation of
    // TestingStabilityMetricsProvider, so this will log a system crash.
    recorder.LogCrash(unclean_time);

    // Record a crash with no system crash.
    recorder.LogCrash(unclean_time - base::Minutes(1));
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

#endif

}  // namespace metrics
