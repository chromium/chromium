// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_metrics_reporter_android.h"

#include <signal.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crash_reporter {

class CrashMetricsReporterObserver : public CrashMetricsReporter::Observer {
 public:
  CrashMetricsReporterObserver() = default;

  CrashMetricsReporterObserver(const CrashMetricsReporterObserver&) = delete;
  CrashMetricsReporterObserver& operator=(const CrashMetricsReporterObserver&) =
      delete;

  ~CrashMetricsReporterObserver() = default;

  // CrashMetricsReporter::Observer:
  void OnCrashDumpProcessed(int rph_id,
                            const CrashMetricsReporter::ReportedCrashTypeSet&
                                reported_counts) override {
    recorded_crash_types_ = reported_counts;
    wait_run_loop_.Quit();
  }

  const CrashMetricsReporter::ReportedCrashTypeSet& recorded_crash_types()
      const {
    return recorded_crash_types_;
  }

  void WaitForProcessed() { wait_run_loop_.Run(); }

 private:
  base::RunLoop wait_run_loop_;
  CrashMetricsReporter::ReportedCrashTypeSet recorded_crash_types_;
};

class CrashMetricsReporterTest : public testing::Test {
 public:
  CrashMetricsReporterTest()
      : scoped_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  CrashMetricsReporterTest(const CrashMetricsReporterTest&) = delete;
  CrashMetricsReporterTest& operator=(const CrashMetricsReporterTest&) = delete;

  ~CrashMetricsReporterTest() override {}

 protected:
  blink::OomInterventionMetrics InterventionMetrics(bool allocation_failed) {
    blink::OomInterventionMetrics metrics;
    metrics.allocation_failed = allocation_failed;
    metrics.current_available_memory_kb = 100;
    metrics.current_swap_free_kb = 0;
    return metrics;
  }

  void TestOomCrashProcessing(
      const ChildExitObserver::TerminationInfo& termination_info,
      CrashMetricsReporter::ReportedCrashTypeSet expected_crash_types,
      const char* histogram_name) {
    base::HistogramTester histogram_tester;

    CrashMetricsReporterObserver crash_dump_observer;
    CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);

    CrashMetricsReporter::GetInstance()->ChildProcessExited(termination_info);
    crash_dump_observer.WaitForProcessed();

    EXPECT_EQ(expected_crash_types, crash_dump_observer.recorded_crash_types());

    if (histogram_name) {
      histogram_tester.ExpectUniqueSample(
          histogram_name, CrashMetricsReporter::EMPTY_MINIDUMP_WHILE_RUNNING,
          1);
    }
    CrashMetricsReporter::GetInstance()->RemoveObserver(&crash_dump_observer);
  }

 private:
  base::test::TaskEnvironment scoped_environment_;
};

TEST_F(CrashMetricsReporterTest, UtilityProcessOOM) {
  ChildExitObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_UTILITY;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = false;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.renderer_has_visible_clients = true;

  TestOomCrashProcessing(
      termination_info,
      {CrashMetricsReporter::ProcessedCrashCounts::kUtilityForegroundOom},
      nullptr);
}

TEST_F(CrashMetricsReporterTest, NormalTerminationIsNotOOMUtilityProcess) {
  ChildExitObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_UTILITY;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = true;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.renderer_has_visible_clients = true;

  TestOomCrashProcessing(termination_info, {}, nullptr);
}

TEST_F(CrashMetricsReporterTest, UtilityProcessAll) {
  ChildExitObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_UTILITY;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.crash_signo = SIGSEGV;
  termination_info.normal_termination = false;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.renderer_has_visible_clients = true;

  CrashMetricsReporterObserver crash_dump_observer;
  CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);

  CrashMetricsReporter::GetInstance()->ChildProcessExited(termination_info);
  crash_dump_observer.WaitForProcessed();

  EXPECT_EQ(CrashMetricsReporter::ReportedCrashTypeSet(
                {CrashMetricsReporter::ProcessedCrashCounts::kUtilityCrashAll}),
            crash_dump_observer.recorded_crash_types());
  CrashMetricsReporter::GetInstance()->RemoveObserver(&crash_dump_observer);
}

TEST_F(CrashMetricsReporterTest, NormalTerminationIsNotOOM) {
  ChildExitObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = true;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.renderer_has_visible_clients = true;
  termination_info.blink_oom_metrics = InterventionMetrics(false);
  TestOomCrashProcessing(termination_info,
                         {CrashMetricsReporter::ProcessedCrashCounts::
                              kRendererForegroundVisibleNormalTermNoMinidump},
                         nullptr);
}

TEST_F(CrashMetricsReporterTest, RendererForegroundCrash) {
  ChildExitObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.crash_signo = SIGSEGV;
  termination_info.normal_termination = true;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = true;
  termination_info.renderer_has_visible_clients = true;

  CrashMetricsReporterObserver crash_dump_observer;
  CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);

  CrashMetricsReporter::GetInstance()->ChildProcessExited(termination_info);
  crash_dump_observer.WaitForProcessed();

  EXPECT_EQ(
      CrashMetricsReporter::ReportedCrashTypeSet(
          {CrashMetricsReporter::ProcessedCrashCounts::
               kRendererForegroundIntentionalKill,
           CrashMetricsReporter::ProcessedCrashCounts::
               kRendererForegroundVisibleCrash,
           CrashMetricsReporter::ProcessedCrashCounts::kRendererCrashAll}),
      crash_dump_observer.recorded_crash_types());
  CrashMetricsReporter::GetInstance()->RemoveObserver(&crash_dump_observer);
}

TEST_F(CrashMetricsReporterTest, SpareRendererAvailabilityHistograms) {
  using SpareRendererAvailabilityWhenKilled =
      CrashMetricsReporter::SpareRendererAvailabilityWhenKilled;
  constexpr char kKillSpareRendererAvailabilityIntentionalKillUMAName[] =
      "Stability.Android.KillSpareRendererAvailability.IntentionalKill";
  constexpr char kKillSpareRendererAvailabilityOOMUMAName[] =
      "Stability.Android.KillSpareRendererAvailability.OOM";

  base::HistogramTester histogram_tester;

  // Nothing recorded if the termination is not intentional kill or OOM.
  {
    ChildExitObserver::TerminationInfo term_info;
    CrashMetricsReporterObserver crash_dump_observer;
    CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);
    term_info.normal_termination = true;
    CrashMetricsReporter::GetInstance()->ChildProcessExited(term_info);
    crash_dump_observer.WaitForProcessed();
    histogram_tester.ExpectTotalCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName, 0);
    histogram_tester.ExpectTotalCount(kKillSpareRendererAvailabilityOOMUMAName,
                                      0);
    CrashMetricsReporter::GetInstance()->RemoveObserver(&crash_dump_observer);
  }

  // Intentional Kill - Spare killed
  {
    ChildExitObserver::TerminationInfo term_info;
    CrashMetricsReporterObserver crash_dump_observer;
    CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);
    term_info = ChildExitObserver::TerminationInfo{};
    term_info.was_killed_intentionally_by_browser = true;
    term_info.is_spare_renderer = true;
    term_info.has_spare_renderer = true;
    CrashMetricsReporter::GetInstance()->ChildProcessExited(term_info);
    crash_dump_observer.WaitForProcessed();
    histogram_tester.ExpectBucketCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName,
        SpareRendererAvailabilityWhenKilled::kKillSpareRenderer, 1);
    histogram_tester.ExpectTotalCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName, 1);
    histogram_tester.ExpectTotalCount(kKillSpareRendererAvailabilityOOMUMAName,
                                      0);
    CrashMetricsReporter::GetInstance()->RemoveObserver(&crash_dump_observer);
  }

  // Intentional Kill - Non-spare killed, spare available
  {
    ChildExitObserver::TerminationInfo term_info;
    CrashMetricsReporterObserver crash_dump_observer;
    CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);
    term_info = ChildExitObserver::TerminationInfo{};
    term_info.was_killed_intentionally_by_browser = true;
    term_info.is_spare_renderer = false;
    term_info.has_spare_renderer = true;
    CrashMetricsReporter::GetInstance()->ChildProcessExited(term_info);
    crash_dump_observer.WaitForProcessed();
    histogram_tester.ExpectBucketCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName,
        SpareRendererAvailabilityWhenKilled::
            kKillNonSpareRendererWithSpareRender,
        1);
    histogram_tester.ExpectTotalCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName, 2);
    histogram_tester.ExpectTotalCount(kKillSpareRendererAvailabilityOOMUMAName,
                                      0);
    CrashMetricsReporter::GetInstance()->RemoveObserver(&crash_dump_observer);
  }

  // Intentional Kill - Non-spare killed, No spare available
  {
    ChildExitObserver::TerminationInfo term_info;
    CrashMetricsReporterObserver crash_dump_observer;
    CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);
    term_info = ChildExitObserver::TerminationInfo{};
    term_info.was_killed_intentionally_by_browser = true;
    term_info.is_spare_renderer = false;
    term_info.has_spare_renderer = false;
    CrashMetricsReporter::GetInstance()->ChildProcessExited(term_info);
    crash_dump_observer.WaitForProcessed();
    histogram_tester.ExpectBucketCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName,
        SpareRendererAvailabilityWhenKilled::
            kKillNonSpareRendererWithoutSpareRenderer,
        1);
    histogram_tester.ExpectTotalCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName, 3);
    histogram_tester.ExpectTotalCount(kKillSpareRendererAvailabilityOOMUMAName,
                                      0);
    CrashMetricsReporter::GetInstance()->RemoveObserver(&crash_dump_observer);
  }

  // OOM Kill - Non-spare killed, spare available
  {
    ChildExitObserver::TerminationInfo term_info;
    CrashMetricsReporterObserver crash_dump_observer;
    CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);
    term_info = ChildExitObserver::TerminationInfo{};
    term_info.was_killed_intentionally_by_browser = false;
    term_info.is_spare_renderer = false;
    term_info.has_spare_renderer = true;
    CrashMetricsReporter::GetInstance()->ChildProcessExited(term_info);
    crash_dump_observer.WaitForProcessed();
    histogram_tester.ExpectBucketCount(kKillSpareRendererAvailabilityOOMUMAName,
                                       SpareRendererAvailabilityWhenKilled::
                                           kKillNonSpareRendererWithSpareRender,
                                       1);
    histogram_tester.ExpectTotalCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName, 3);
    histogram_tester.ExpectTotalCount(kKillSpareRendererAvailabilityOOMUMAName,
                                      1);
    CrashMetricsReporter::GetInstance()->RemoveObserver(&crash_dump_observer);
  }

  // OOM Kill - Spare killed
  {
    ChildExitObserver::TerminationInfo term_info;
    CrashMetricsReporterObserver crash_dump_observer;
    CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);
    term_info = ChildExitObserver::TerminationInfo{};
    term_info.was_killed_intentionally_by_browser = false;
    term_info.is_spare_renderer = true;
    term_info.has_spare_renderer = true;
    CrashMetricsReporter::GetInstance()->ChildProcessExited(term_info);
    crash_dump_observer.WaitForProcessed();
    histogram_tester.ExpectBucketCount(
        kKillSpareRendererAvailabilityOOMUMAName,
        SpareRendererAvailabilityWhenKilled::kKillSpareRenderer, 1);
    histogram_tester.ExpectTotalCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName, 3);
    histogram_tester.ExpectTotalCount(kKillSpareRendererAvailabilityOOMUMAName,
                                      2);
    CrashMetricsReporter::GetInstance()->RemoveObserver(&crash_dump_observer);
  }

  // OOM Kill - Non-spare killed, No spare available
  {
    ChildExitObserver::TerminationInfo term_info;
    CrashMetricsReporterObserver crash_dump_observer;
    CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);
    term_info = ChildExitObserver::TerminationInfo{};
    term_info.was_killed_intentionally_by_browser = false;
    term_info.is_spare_renderer = false;
    term_info.has_spare_renderer = false;
    CrashMetricsReporter::GetInstance()->ChildProcessExited(term_info);
    crash_dump_observer.WaitForProcessed();
    histogram_tester.ExpectBucketCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName,
        SpareRendererAvailabilityWhenKilled::
            kKillNonSpareRendererWithoutSpareRenderer,
        1);
    histogram_tester.ExpectTotalCount(
        kKillSpareRendererAvailabilityIntentionalKillUMAName, 3);
    histogram_tester.ExpectTotalCount(kKillSpareRendererAvailabilityOOMUMAName,
                                      3);
    CrashMetricsReporter::GetInstance()->RemoveObserver(&crash_dump_observer);
  }
}

}  // namespace crash_reporter
