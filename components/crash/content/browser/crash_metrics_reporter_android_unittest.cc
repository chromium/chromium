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
    metrics.current_private_footprint_kb = 100;
    metrics.current_swap_kb = 0;
    metrics.current_vm_size_kb = 0;
    metrics.current_blink_usage_kb = 0;
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

}  // namespace crash_reporter
