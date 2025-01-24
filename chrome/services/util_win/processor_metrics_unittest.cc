// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/processor_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ProcessorMetricsTest, TestRecordProcessorMetrics) {
  base::HistogramTester histogram_tester;

  RecordProcessorMetricsForTesting();

  histogram_tester.ExpectTotalCount("Windows.ProcessorFamily", 1);
  histogram_tester.ExpectTotalCount(
      "Windows.ProcessorVirtualizationFirmwareEnabled", 1);
  histogram_tester.ExpectTotalCount("Windows.HypervPresent", 1);
  histogram_tester.ExpectTotalCount("Windows.CetAvailable", 1);
}
