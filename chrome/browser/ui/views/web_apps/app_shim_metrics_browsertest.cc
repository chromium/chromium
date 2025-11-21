// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/views/web_apps/web_app_integration_test_driver.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace web_app::integration_tests {
namespace {

void FetchHistogramsFromChildProcesses() {
  content::FetchHistogramsFromChildProcesses();
  // Do an "async" merging if histogram deltas to make sure that any previously
  // posted but not yet ran merge operations (such as those triggered by the app
  // shim terminating) have also completed. A sync merge operation merely gets
  // data from still existing subprocesses and does not ensure that data from
  // already destroyed subpresses also gets merged.
  base::test::TestFuture<void> future;
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting(
      /*async=*/true, future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

using AppShimMetricsTest = WebAppIntegrationTest;

IN_PROC_BROWSER_TEST_F(AppShimMetricsTest, Basics) {
  base::HistogramTester histogram_tester;
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();

  FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectTotalCount("AppShim.Launched",
                                    /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("AppShim.WillTerminate",
                                    /*expected_count=*/0);

  helper_.QuitAppShim(Site::kStandalone);
  helper_.CheckWindowClosed();

  // After quitting we should have metrics from it.
  FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectTotalCount("AppShim.Launched",
                                    /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("AppShim.WillTerminate",
                                    /*expected_count=*/1);
}

}  // namespace
}  // namespace web_app::integration_tests
