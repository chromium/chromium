// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace regional_capabilities {

TEST(RegionalCapabilitiesMetricsTest, RecordActiveRegionalProgram_Empty) {
  base::HistogramTester histogram_tester;
  RecordActiveRegionalProgram({});
  histogram_tester.ExpectUniqueSample(
      "RegionalCapabilities.Debug.HasActiveRegionalProgram", false, 1);
  histogram_tester.ExpectTotalCount(
      "RegionalCapabilities.ActiveRegionalProgram2", 0);
}

TEST(RegionalCapabilitiesMetricsTest, RecordActiveRegionalProgram_Default) {
  base::HistogramTester histogram_tester;
  RecordActiveRegionalProgram({ActiveRegionalProgram::kDefault});
  histogram_tester.ExpectUniqueSample(
      "RegionalCapabilities.Debug.HasActiveRegionalProgram", true, 1);
  histogram_tester.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kDefault, 1);
}

TEST(RegionalCapabilitiesMetricsTest,
     RecordActiveRegionalProgram_OneNonDefault) {
  base::HistogramTester histogram_tester;
  RecordActiveRegionalProgram({ActiveRegionalProgram::kWaffle});
  histogram_tester.ExpectUniqueSample(
      "RegionalCapabilities.Debug.HasActiveRegionalProgram", true, 1);
  histogram_tester.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kWaffle, 1);
}

TEST(RegionalCapabilitiesMetricsTest,
     RecordActiveRegionalProgram_MultipleNonDefault) {
  base::HistogramTester histogram_tester;
  RecordActiveRegionalProgram(
      {ActiveRegionalProgram::kWaffle, ActiveRegionalProgram::kTaiyaki});
  histogram_tester.ExpectUniqueSample(
      "RegionalCapabilities.Debug.HasActiveRegionalProgram", true, 1);
  histogram_tester.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kMixed, 1);
}

TEST(RegionalCapabilitiesMetricsTest,
     RecordActiveRegionalProgram_DefaultAndNonDefault) {
  base::HistogramTester histogram_tester;
  RecordActiveRegionalProgram(
      {ActiveRegionalProgram::kDefault, ActiveRegionalProgram::kWaffle});
  histogram_tester.ExpectUniqueSample(
      "RegionalCapabilities.Debug.HasActiveRegionalProgram", true, 1);
  histogram_tester.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kWaffle, 1);
}

}  // namespace regional_capabilities
