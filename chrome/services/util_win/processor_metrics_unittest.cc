// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/processor_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

// This test may be flaky and may need to be disabled.
// See crbug.com/1049212
TEST(ProcessorMetricsTest, TestRecordProcessorMetrics) {
  // This metric cannot be collected on versions under Windows 10 since the
  // WMI fields needed to log this only exist in Windows 10 and above.
  if (base::win::OSInfo::GetInstance()->version() < base::win::Version::WIN10)
    return;

  base::win::ScopedCOMInitializer scoped_com_initializer;
  base::HistogramTester histogram_tester;

  RecordProcessorMetricsForTesting();

  histogram_tester.ExpectTotalCount("Windows.ProcessorFamily", 1);
  histogram_tester.ExpectTotalCount(
      "Windows.ProcessorVirtualizationFirmwareEnabled", 1);
  histogram_tester.ExpectTotalCount("Windows.HypervPresent", 1);
  histogram_tester.ExpectTotalCount("Windows.CetAvailable", 1);
}
