// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/statistics_collector.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kReloadPoliciesHistogram[] =
    "Enterprise.PolicyUI.ButtonUsage.ReloadPolicies";
constexpr char kCopyToJsonHistogram[] =
    "Enterprise.PolicyUI.ButtonUsage.CopyToJson";
#if !BUILDFLAG(IS_IOS)
constexpr char kExportToJsonHistogram[] =
    "Enterprise.PolicyUI.ButtonUsage.ExportToJson";
#endif
constexpr char kUploadReportHistogram[] =
    "Enterprise.PolicyUI.ButtonUsage.UploadReport";

constexpr uint32_t kReloadPoliciesCount = 5;
constexpr uint32_t kExportToJsonCount = 3;
constexpr uint32_t kCopyToJsonCount = 7;
constexpr uint32_t kUploadReportCount = 2;
constexpr uint32_t kZeroCount = 0;

}  // namespace

TEST(PolicyWebUIStatisticsCollectorTest, RecordPolicyUIButtonUsage) {
  base::HistogramTester histogram_tester;

  RecordPolicyUIButtonUsage(kReloadPoliciesCount, kExportToJsonCount,
                            kCopyToJsonCount, kUploadReportCount);

  histogram_tester.ExpectUniqueSample(kReloadPoliciesHistogram,
                                      kReloadPoliciesCount, 1);
  histogram_tester.ExpectUniqueSample(kCopyToJsonHistogram, kCopyToJsonCount,
                                      1);
#if !BUILDFLAG(IS_IOS)
  histogram_tester.ExpectUniqueSample(kExportToJsonHistogram,
                                      kExportToJsonCount, 1);
#endif
  histogram_tester.ExpectUniqueSample(kUploadReportHistogram,
                                      kUploadReportCount, 1);
}

TEST(PolicyWebUIStatisticsCollectorTest, RecordZeroCounts) {
  base::HistogramTester histogram_tester;

  RecordPolicyUIButtonUsage(kZeroCount, kZeroCount, kZeroCount, kZeroCount);

  histogram_tester.ExpectUniqueSample(kReloadPoliciesHistogram, kZeroCount, 1);
  histogram_tester.ExpectUniqueSample(kCopyToJsonHistogram, kZeroCount, 1);
#if !BUILDFLAG(IS_IOS)
  histogram_tester.ExpectUniqueSample(kExportToJsonHistogram, kZeroCount, 1);
#endif
  histogram_tester.ExpectUniqueSample(kUploadReportHistogram, kZeroCount, 1);
}

TEST(PolicyWebUIStatisticsCollectorTest, MultipleRecords) {
  base::HistogramTester histogram_tester;

  RecordPolicyUIButtonUsage(kReloadPoliciesCount, kExportToJsonCount,
                            kCopyToJsonCount, kUploadReportCount);
  RecordPolicyUIButtonUsage(kZeroCount, kZeroCount, kZeroCount, kZeroCount);

  histogram_tester.ExpectTotalCount(kReloadPoliciesHistogram, 2);
  histogram_tester.ExpectBucketCount(kReloadPoliciesHistogram,
                                     kReloadPoliciesCount, 1);
  histogram_tester.ExpectBucketCount(kReloadPoliciesHistogram, kZeroCount, 1);

  histogram_tester.ExpectTotalCount(kCopyToJsonHistogram, 2);
  histogram_tester.ExpectBucketCount(kCopyToJsonHistogram, kCopyToJsonCount, 1);
  histogram_tester.ExpectBucketCount(kCopyToJsonHistogram, kZeroCount, 1);

#if !BUILDFLAG(IS_IOS)
  histogram_tester.ExpectTotalCount(kExportToJsonHistogram, 2);
  histogram_tester.ExpectBucketCount(kExportToJsonHistogram, kExportToJsonCount,
                                     1);
  histogram_tester.ExpectBucketCount(kExportToJsonHistogram, kZeroCount, 1);
#endif

  histogram_tester.ExpectTotalCount(kUploadReportHistogram, 2);
  histogram_tester.ExpectBucketCount(kUploadReportHistogram, kUploadReportCount,
                                     1);
  histogram_tester.ExpectBucketCount(kUploadReportHistogram, kZeroCount, 1);
}

}  // namespace policy
