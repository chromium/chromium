// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/javascript_dropdown_metrics.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

TEST(JavaScriptDropdownMetricsTest, Detected) {
  base::HistogramTester histogram_tester;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const std::vector<JavaScriptFieldModification> field_modifications = {
      {.modification_type = mojom::JavaScriptModificationType::kReassignment,
       .timestamp = start_time + base::Milliseconds(10)},
      {.modification_type = mojom::JavaScriptModificationType::kReassignment,
       .timestamp = start_time + base::Milliseconds(20)},
      {.modification_type = mojom::JavaScriptModificationType::kReassignment,
       .timestamp = start_time + base::Milliseconds(35)}};

  LogJavaScriptDropdownDetectionMetrics(JavaScriptDropdownType::kAddress,
                                        field_modifications, start_time);

  histogram_tester.ExpectUniqueSample("Autofill.DropdownDetection.DetectedType",
                                      JavaScriptDropdownType::kAddress, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.DropdownDetection.JavaScriptModificationTime.First",
      base::Milliseconds(10), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.DropdownDetection.JavaScriptModificationTime.Last",
      base::Milliseconds(35), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.DropdownDetection.JavaScriptModificationTime.Duration",
      base::Milliseconds(25), 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.DropdownDetection.JavaScriptModificationTime", 3);
  histogram_tester.ExpectTimeBucketCount(
      "Autofill.DropdownDetection.JavaScriptModificationTime",
      base::Milliseconds(10), 1);
  histogram_tester.ExpectTimeBucketCount(
      "Autofill.DropdownDetection.JavaScriptModificationTime",
      base::Milliseconds(20), 1);
  histogram_tester.ExpectTimeBucketCount(
      "Autofill.DropdownDetection.JavaScriptModificationTime",
      base::Milliseconds(35), 1);
}

TEST(JavaScriptDropdownMetricsTest, None) {
  base::HistogramTester histogram_tester;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const std::vector<JavaScriptFieldModification> field_modifications = {
      {.modification_type = mojom::JavaScriptModificationType::kReassignment,
       .timestamp = start_time + base::Milliseconds(10)}};

  LogJavaScriptDropdownDetectionMetrics(JavaScriptDropdownType::kNone,
                                        field_modifications, start_time);

  histogram_tester.ExpectUniqueSample("Autofill.DropdownDetection.DetectedType",
                                      JavaScriptDropdownType::kNone, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.DropdownDetection.JavaScriptModificationTime.First", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.DropdownDetection.JavaScriptModificationTime.Last", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.DropdownDetection.JavaScriptModificationTime.Duration", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.DropdownDetection.JavaScriptModificationTime", 0);
}

}  // namespace
}  // namespace autofill::autofill_metrics
