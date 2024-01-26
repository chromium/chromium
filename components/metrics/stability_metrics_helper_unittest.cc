// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_helper.h"

#include <memory>

#include "base/process/kill.h"
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

#if BUILDFLAG(IS_IOS)
TEST_F(StabilityMetricsHelperTest, LogRendererCrash) {
  StabilityMetricsHelper helper(prefs());
  base::HistogramTester histogram_tester;

  helper.LogRendererCrash();

  constexpr int kDummyExitCode = 105;
  histogram_tester.ExpectUniqueSample("CrashExitCodes.Renderer", kDummyExitCode,
                                      1);
  histogram_tester.ExpectBucketCount("BrowserRenderProcessHost.ChildCrashes",
                                     RENDERER_TYPE_RENDERER, 1);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kRendererCrash, 1);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kExtensionCrash, 0);
}
#elif !BUILDFLAG(IS_ANDROID)
TEST_F(StabilityMetricsHelperTest, LogRendererCrash) {
  StabilityMetricsHelper helper(prefs());
  base::HistogramTester histogram_tester;

  // Crash and abnormal termination should increment renderer crash count.
  helper.LogRendererCrash(RendererHostedContentType::kForegroundMainFrame,
                          base::TERMINATION_STATUS_PROCESS_CRASHED, 1);

  helper.LogRendererCrash(RendererHostedContentType::kForegroundMainFrame,
                          base::TERMINATION_STATUS_ABNORMAL_TERMINATION, 1);

  // OOM should increment renderer crash count.
  helper.LogRendererCrash(RendererHostedContentType::kForegroundMainFrame,
                          base::TERMINATION_STATUS_OOM, 1);

  // Kill does not increment renderer crash count.
  helper.LogRendererCrash(RendererHostedContentType::kForegroundMainFrame,
                          base::TERMINATION_STATUS_PROCESS_WAS_KILLED, 1);

  // Failed launch increments failed launch count.
  helper.LogRendererCrash(RendererHostedContentType::kForegroundMainFrame,
                          base::TERMINATION_STATUS_LAUNCH_FAILED, 1);

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
#endif

// Note: ENABLE_EXTENSIONS is set to false in Android
#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(StabilityMetricsHelperTest, LogRendererCrashEnableExtensions) {
  StabilityMetricsHelper helper(prefs());
  base::HistogramTester histogram_tester;

  // Crash and abnormal termination should increment extension crash count.
  helper.LogRendererCrash(RendererHostedContentType::kExtension,
                          base::TERMINATION_STATUS_PROCESS_CRASHED, 1);

  // OOM should increment extension renderer crash count.
  helper.LogRendererCrash(RendererHostedContentType::kExtension,
                          base::TERMINATION_STATUS_OOM, 1);

  // Failed launch increments extension failed launch count.
  helper.LogRendererCrash(RendererHostedContentType::kExtension,
                          base::TERMINATION_STATUS_LAUNCH_FAILED, 1);

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
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Verifies that "Stability.RendererAbnormalTermination2.*" histograms are
// correctly recorded by `LogRendererCrash`.
TEST_F(StabilityMetricsHelperTest, RendererAbnormalTerminationCount) {
  StabilityMetricsHelper helper(prefs());

  {
    // Normal termination does not record anything.
    base::HistogramTester tester;
    helper.LogRendererCrash(
        RendererHostedContentType::kForegroundMainFrame,
        base::TerminationStatus::TERMINATION_STATUS_NORMAL_TERMINATION, 0);
    EXPECT_EQ("", tester.GetAllHistogramsRecorded());
  }

  // Test all abnormal termination status / hosted content type combinations.
  for (auto status : {
           base::TERMINATION_STATUS_ABNORMAL_TERMINATION,
           base::TERMINATION_STATUS_PROCESS_WAS_KILLED,
           base::TERMINATION_STATUS_PROCESS_CRASHED,
           base::TERMINATION_STATUS_STILL_RUNNING,
#if BUILDFLAG(IS_CHROMEOS)
           base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM,
#endif
           base::TERMINATION_STATUS_LAUNCH_FAILED,
           base::TERMINATION_STATUS_OOM,
#if BUILDFLAG(IS_WIN)
           base::TERMINATION_STATUS_INTEGRITY_FAILURE,
#endif
       }) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {
      base::HistogramTester tester;
      helper.LogRendererCrash(RendererHostedContentType::kExtension, status, 0);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.HostedContentType",
          RendererHostedContentType::kExtension, 1);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.Extension", status, 1);
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

    {
      base::HistogramTester tester;
      helper.LogRendererCrash(RendererHostedContentType::kForegroundMainFrame,
                              status, 0);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.HostedContentType",
          RendererHostedContentType::kForegroundMainFrame, 1);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.ForegroundMainFrame", status,
          1);
    }

    {
      base::HistogramTester tester;
      helper.LogRendererCrash(RendererHostedContentType::kForegroundSubframe,
                              status, 0);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.HostedContentType",
          RendererHostedContentType::kForegroundSubframe, 1);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.ForegroundSubframe", status,
          1);
    }

    {
      base::HistogramTester tester;
      helper.LogRendererCrash(RendererHostedContentType::kBackgroundFrame,
                              status, 0);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.HostedContentType",
          RendererHostedContentType::kBackgroundFrame, 1);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.BackgroundFrame", status, 1);
    }

    {
      base::HistogramTester tester;
      helper.LogRendererCrash(RendererHostedContentType::kInactiveFrame, status,
                              0);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.HostedContentType",
          RendererHostedContentType::kInactiveFrame, 1);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.InactiveFrame", status, 1);
    }

    {
      base::HistogramTester tester;
      helper.LogRendererCrash(RendererHostedContentType::kNoFrameOrExtension,
                              status, 0);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.HostedContentType",
          RendererHostedContentType::kNoFrameOrExtension, 1);
      tester.ExpectUniqueSample(
          "Stability.RendererAbnormalTermination2.NoFrameOrExtension", status,
          1);
    }
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace metrics
