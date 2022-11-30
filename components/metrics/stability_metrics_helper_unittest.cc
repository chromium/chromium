// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_helper.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

namespace {

enum RendererType {
  RENDERER_TYPE_RENDERER = 1,
  RENDERER_TYPE_EXTENSION,
  // NOTE: Add new action types only immediately above this line. Also,
  // make sure the enum list in tools/metrics/histograms/histograms.xml is
  // updated with any change in here.
  RENDERER_TYPE_COUNT
};

class StabilityMetricsHelperTest : public testing::Test {
 public:
  StabilityMetricsHelperTest(const StabilityMetricsHelperTest&) = delete;
  StabilityMetricsHelperTest& operator=(const StabilityMetricsHelperTest&) =
      delete;

 protected:
  StabilityMetricsHelperTest() : prefs_(new TestingPrefServiceSimple) {
    StabilityMetricsHelper::RegisterPrefs(prefs()->registry());
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
TEST_F(StabilityMetricsHelperTest, LogRendererCrash) {
  StabilityMetricsHelper helper(prefs());
  base::HistogramTester histogram_tester;

  // Crash and abnormal termination should increment renderer crash count.
  helper.LogRendererCrash(false, base::TERMINATION_STATUS_PROCESS_CRASHED, 1);

  helper.LogRendererCrash(false, base::TERMINATION_STATUS_ABNORMAL_TERMINATION,
                          1);

  // OOM should increment renderer crash count.
  helper.LogRendererCrash(false, base::TERMINATION_STATUS_OOM, 1);

  // Kill does not increment renderer crash count.
  helper.LogRendererCrash(false, base::TERMINATION_STATUS_PROCESS_WAS_KILLED,
                          1);

  // Failed launch increments failed launch count.
  helper.LogRendererCrash(false, base::TERMINATION_STATUS_LAUNCH_FAILED, 1);

  histogram_tester.ExpectUniqueSample("CrashExitCodes.Renderer", 1, 3);
  histogram_tester.ExpectBucketCount("BrowserRenderProcessHost.ChildCrashes",
                                     RENDERER_TYPE_RENDERER, 3);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kRendererCrash, 3);
  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", StabilityEventType::kRendererFailedLaunch, 1);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kExtensionCrash, 0);

  // One launch failure each.
  histogram_tester.ExpectBucketCount(
      "BrowserRenderProcessHost.ChildLaunchFailures", RENDERER_TYPE_RENDERER,
      1);

  // TERMINATION_STATUS_PROCESS_WAS_KILLED for a renderer.
  histogram_tester.ExpectBucketCount("BrowserRenderProcessHost.ChildKills",
                                     RENDERER_TYPE_RENDERER, 1);
  histogram_tester.ExpectBucketCount("BrowserRenderProcessHost.ChildKills",
                                     RENDERER_TYPE_EXTENSION, 0);
  histogram_tester.ExpectBucketCount(
      "BrowserRenderProcessHost.ChildLaunchFailureCodes", 1, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Note: ENABLE_EXTENSIONS is set to false in Android
#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(StabilityMetricsHelperTest, LogRendererCrashEnableExtensions) {
  StabilityMetricsHelper helper(prefs());
  base::HistogramTester histogram_tester;

  // Crash and abnormal termination should increment extension crash count.
  helper.LogRendererCrash(true, base::TERMINATION_STATUS_PROCESS_CRASHED, 1);

  // OOM should increment extension renderer crash count.
  helper.LogRendererCrash(true, base::TERMINATION_STATUS_OOM, 1);

  // Failed launch increments extension failed launch count.
  helper.LogRendererCrash(true, base::TERMINATION_STATUS_LAUNCH_FAILED, 1);

  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kRendererCrash, 0);
  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", StabilityEventType::kExtensionRendererFailedLaunch,
      1);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kExtensionCrash, 2);

  histogram_tester.ExpectBucketCount(
      "BrowserRenderProcessHost.ChildLaunchFailureCodes", 1, 1);
  histogram_tester.ExpectUniqueSample("CrashExitCodes.Extension", 1, 2);
  histogram_tester.ExpectBucketCount("BrowserRenderProcessHost.ChildCrashes",
                                     RENDERER_TYPE_EXTENSION, 2);
  histogram_tester.ExpectBucketCount(
      "BrowserRenderProcessHost.ChildLaunchFailures", RENDERER_TYPE_EXTENSION,
      1);
}
#endif

}  // namespace metrics
