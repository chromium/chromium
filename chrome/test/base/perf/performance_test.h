// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_PERF_PERFORMANCE_TEST_H_
#define CHROME_TEST_BASE_PERF_PERFORMANCE_TEST_H_

#include "base/time/time.h"
#include "chrome/test/base/in_process_browser_test.h"

// PerformanceTest is an interactive-ui-test that can be used to collect traces.
// The traces can then be post-processed, e.g. using catapult, to report
// performance metrics. The GetUMAHistogramNames() and GetTracingCategories()
// overrides can be used to specify which trace-events and which UMA histograms
// should be collected in the trace files. The trace-file location must be set
// using the --trace-dir=<path> command-line flag. If the flag is not given,
// then the test will still run, but not produce any traces.
class PerformanceTest : public InProcessBrowserTest {
 public:
  PerformanceTest();
  ~PerformanceTest() override;

  virtual std::vector<std::string> GetUMAHistogramNames() const;
  virtual const std::string GetTracingCategories() const;
  // Returns the names of timeline based metrics (TBM) to be extracted from
  // the generated trace. The metrics must be defined in telemetry
  //   third_party/catapult/tracing/tracing/metrics/
  // so that third_party/catapult/tracing/bin/run_metric could handle them.
  virtual std::vector<std::string> GetTimelineBasedMetrics() const;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  // Returns the mean of the histogram |name|.
  float GetHistogramMean(const std::string& name);

  bool HasHistogram(const std::string& name);

  const bool should_start_trace_;

  // Tracks whether SetUpOnMainThread was called. Ensures subclasses remember to
  // call the base classes SetupOnMainThread.
  bool setup_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(PerformanceTest);
};

// UIPerformanceTest is specifically to be used for measuring ui-related
// performance. It turns on the appropriate tracing categories for ui.
class UIPerformanceTest : public PerformanceTest {
 public:
  UIPerformanceTest() = default;
  ~UIPerformanceTest() override = default;

  // PerformanceTest:
  void SetUpOnMainThread() override;

  const std::string GetTracingCategories() const override;
  // Default is "renderingMetric", "umaMetric".
  std::vector<std::string> GetTimelineBasedMetrics() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UIPerformanceTest);
};

#endif  // CHROME_TEST_BASE_PERF_PERFORMANCE_TEST_H_
