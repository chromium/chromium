// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/web_apps/web_app_integration_test_driver.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace web_app::integration_tests {
namespace {

void FetchHistogramsFromChildProcesses() {
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
}

using AppShimMetricsTest = WebAppIntegrationTest;

IN_PROC_BROWSER_TEST_F(AppShimMetricsTest, Basics) {
  base::HistogramTester histogram_tester;
  helper_.InstallMenuOption(InstallableSite::kStandalone);
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
