// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/statistics_collector.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"

namespace policy {

namespace {

constexpr int kMetricsCountMin = 0;
constexpr int kMetricsCountMax = 51;
constexpr int kMetricsBucket = 52;

}  // namespace

void RecordPolicyUIButtonUsage(uint32_t reload_policies_count,
                               uint32_t export_to_json_count,
                               uint32_t copy_to_json_count,
                               uint32_t upload_report_count) {
  base::UmaHistogramCustomCounts(
      "Enterprise.PolicyUI.ButtonUsage.ReloadPolicies", reload_policies_count,
      kMetricsCountMin, kMetricsCountMax, kMetricsBucket);
  base::UmaHistogramCustomCounts("Enterprise.PolicyUI.ButtonUsage.CopyToJson",
                                 copy_to_json_count, kMetricsCountMin,
                                 kMetricsCountMax, kMetricsBucket);
#if !BUILDFLAG(IS_IOS)
  base::UmaHistogramCustomCounts("Enterprise.PolicyUI.ButtonUsage.ExportToJson",
                                 export_to_json_count, kMetricsCountMin,
                                 kMetricsCountMax, kMetricsBucket);
#endif
  base::UmaHistogramCustomCounts("Enterprise.PolicyUI.ButtonUsage.UploadReport",
                                 upload_report_count, kMetricsCountMin,
                                 kMetricsCountMax, kMetricsBucket);
}

}  // namespace policy
