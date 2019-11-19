// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_metrics_provider_win.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>

#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

namespace {

using GetExecutableDetailsCallback =
    WatcherMetricsProviderWin::GetExecutableDetailsCallback;

const wchar_t kRegistryPath[] = L"Software\\WatcherMetricsProviderWinTest";

class WatcherMetricsProviderWinTest : public testing::Test {
 public:
  typedef testing::Test Super;

  void SetUp() override {
    Super::SetUp();

    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  void AddProcessExitCode(bool use_own_pid, int exit_code) {
    int pid = 0;
    if (use_own_pid) {
      pid = base::GetCurrentProcId();
    } else {
      // Make sure not to accidentally collide with own pid.
      do {
        pid = rand();
      } while (pid == static_cast<int>(base::GetCurrentProcId()));
    }

    base::win::RegKey key(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE);

    // Make up a unique key, starting with the given pid.
    base::string16 key_name(base::StringPrintf(L"%d-%d", pid, rand()));

    // Write the exit code to registry.
    LONG result = key.WriteValue(key_name.c_str(), exit_code);
    ASSERT_EQ(result, ERROR_SUCCESS);
  }

  size_t ExitCodeRegistryPathValueCount() {
    base::win::RegKey key(HKEY_CURRENT_USER, kRegistryPath, KEY_READ);
    return key.GetValueCount();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager override_manager_;
  base::HistogramTester histogram_tester_;
};

}  // namespace

TEST_F(WatcherMetricsProviderWinTest, RecordsStabilityHistogram) {
  // Record multiple success exits.
  for (size_t i = 0; i < 11; ++i)
    AddProcessExitCode(false, 0);

  // Record a single failure.
  AddProcessExitCode(false, 100);

  WatcherMetricsProviderWin provider(kRegistryPath, base::FilePath(),
                                     base::FilePath(),
                                     GetExecutableDetailsCallback());

  provider.ProvideStabilityMetrics(nullptr);
  histogram_tester_.ExpectBucketCount(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 0, 11);
  histogram_tester_.ExpectBucketCount(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 100, 1);
  histogram_tester_.ExpectTotalCount(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 12);

  // Verify that the reported values are gone.
  EXPECT_EQ(0u, ExitCodeRegistryPathValueCount());
}

TEST_F(WatcherMetricsProviderWinTest, DoesNotReportOwnProcessId) {
  // Record multiple success exits.
  for (size_t i = 0; i < 11; ++i)
    AddProcessExitCode(i, 0);

  // Record own process as STILL_ACTIVE.
  AddProcessExitCode(true, STILL_ACTIVE);

  WatcherMetricsProviderWin provider(kRegistryPath, base::FilePath(),
                                     base::FilePath(),
                                     GetExecutableDetailsCallback());

  provider.ProvideStabilityMetrics(nullptr);
  histogram_tester_.ExpectUniqueSample(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 0, 11);

  // Verify that the reported values are gone.
  EXPECT_EQ(1u, ExitCodeRegistryPathValueCount());
}

TEST_F(WatcherMetricsProviderWinTest, DeletesExitcodeKeyWhenNotReporting) {
  // Test that the registry at kRegistryPath is deleted when reporting is
  // disabled.

  // Record multiple success exits.
  for (size_t i = 0; i < 11; ++i)
    AddProcessExitCode(false, 0);
  // Record a single failure.
  AddProcessExitCode(false, 100);

  // Verify that the key exists prior to deletion.
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, kRegistryPath, KEY_READ));

  // Make like the user is opted out of reporting.
  WatcherMetricsProviderWin provider(kRegistryPath, base::FilePath(),
                                     base::FilePath(),
                                     GetExecutableDetailsCallback());
  provider.OnRecordingDisabled();

  // Flush the task(s).
  task_environment_.RunUntilIdle();

  // Make sure the subkey for the pseudo process has been deleted on reporting.
  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            key.Open(HKEY_CURRENT_USER, kRegistryPath, KEY_READ));
}

}  // namespace browser_watcher
