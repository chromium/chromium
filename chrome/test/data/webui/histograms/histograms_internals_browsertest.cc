// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"

class HistogramsInternalsUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  HistogramsInternalsUIBrowserTest() {
    set_test_loader_host(content::kChromeUIHistogramHost);
  }

  void RunTestCase(const std::string& testCase) {
    RunTestWithoutTestLoader(
        "histograms/histograms_internals_test.js",
        base::StringPrintf("runMochaTest('HistogramsInternals', '%s');",
                           testCase.c_str()));
  }

  void PopulateHistograms() {
    base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
        "HTMLOut", /*minimum=*/1, /*maximum=*/10,
        /*bucket_count=*/5, base::HistogramBase::kNoFlags);
    histogram->AddCount(/*value=*/4, /*count=*/5);
    base::HistogramBase* histogram1 = base::LinearHistogram::FactoryGet(
        "HTMLOut1", /*minimum=*/1, /*maximum=*/20,
        /*bucket_count=*/4, base::HistogramBase::kNoFlags);
    histogram1->AddCount(/*value=*/10, /*count=*/2);
    histogram1->AddCount(/*value=*/15, /*count=*/4);
  }
};

IN_PROC_BROWSER_TEST_F(HistogramsInternalsUIBrowserTest, RefreshHistograms) {
  PopulateHistograms();
  RunTestCase("RefreshHistograms");
}

IN_PROC_BROWSER_TEST_F(HistogramsInternalsUIBrowserTest, NoDummyHistograms) {
  RunTestCase("NoDummyHistograms");
}

IN_PROC_BROWSER_TEST_F(HistogramsInternalsUIBrowserTest, DownloadHistograms) {
  PopulateHistograms();
  RunTestCase("DownloadHistograms");
}

IN_PROC_BROWSER_TEST_F(HistogramsInternalsUIBrowserTest, StopMonitoring) {
  RunTestCase("StopMonitoring");
}

IN_PROC_BROWSER_TEST_F(HistogramsInternalsUIBrowserTest, SubprocessCheckbox) {
  RunTestCase("SubprocessCheckbox");
}

IN_PROC_BROWSER_TEST_F(HistogramsInternalsUIBrowserTest,
                       SubprocessCheckboxInMonitoringMode) {
  RunTestCase("SubprocessCheckboxInMonitoringMode");
}
