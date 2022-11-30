// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow_status_reporter.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace attestation {

namespace {

bool IsValidEntry(int entry) {
  // We only care about the fact if the flows are run or not.
  entry &= 0b1111;
  // There has to be at least 1 flow that runs.
  if (!(entry & 0b1010)) {
    return false;
  }
  // The fallback flow is reported to be successful, but not run.
  if ((entry & 0b11) == 0b01) {
    return false;
  }
  // The default flow is reported to be successful, but not run.
  if (((entry >> 2) & 0b11) == 0b01) {
    return false;
  }
  return true;
}

}  // namespace

TEST(AttestationFlowStatusReporterTest, AllValidCombinations) {
  for (int i = 0; i < (1 << 6); ++i) {
    const int expected_entry = i;
    // The expected entry is legal only if any flow is run.
    if (!IsValidEntry(expected_entry)) {
      continue;
    }
    base::HistogramTester histogram_tester;
    // Put the reporter in an inner loop to report the UMA during destruction.
    {
      AttestationFlowStatusReporter reporter;
      reporter.OnHasProxy(expected_entry & (1 << 5));
      reporter.OnIsSystemProxyAvailable(expected_entry & (1 << 4));
      if (expected_entry & (1 << 3)) {
        reporter.OnDefaultFlowStatus(expected_entry & (1 << 2));
      }
      if (expected_entry & (1 << 1)) {
        reporter.OnFallbackFlowStatus(expected_entry & 1);
      }
    }
    histogram_tester.ExpectUniqueSample(
        "ChromeOS.Attestation.AttestationFlowStatus", expected_entry, 1);
  }
}

}  // namespace attestation
}  // namespace ash
