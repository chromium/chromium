// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_engagement/content/site_engagement_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_score.h"

namespace site_engagement {

const char SiteEngagementMetrics::kTotalOriginsHistogram[] =
    "SiteEngagementService.OriginsEngaged";

const char SiteEngagementMetrics::kMeanEngagementHistogram[] =
    "SiteEngagementService.MeanEngagement";

const char SiteEngagementMetrics::kMedianEngagementHistogram[] =
    "SiteEngagementService.MedianEngagement";

const char SiteEngagementMetrics::kEngagementScoreHistogram[] =
    "SiteEngagementService.EngagementScore";

const char SiteEngagementMetrics::kEngagementTypeHistogram[] =
    "SiteEngagementService.EngagementType";

void SiteEngagementMetrics::RecordTotalOriginsEngaged(int num_origins) {
  UMA_HISTOGRAM_COUNTS_10000(kTotalOriginsHistogram, num_origins);
}

void SiteEngagementMetrics::RecordMeanEngagement(double mean_engagement) {
  UMA_HISTOGRAM_COUNTS_100(kMeanEngagementHistogram, mean_engagement);
}

void SiteEngagementMetrics::RecordMedianEngagement(double median_engagement) {
  UMA_HISTOGRAM_COUNTS_100(kMedianEngagementHistogram, median_engagement);
}

void SiteEngagementMetrics::RecordEngagementScores(
    const std::vector<mojom::SiteEngagementDetails>& details) {
  if (details.empty())
    return;

  for (const auto& detail : details) {
    UMA_HISTOGRAM_COUNTS_100(kEngagementScoreHistogram, detail.total_score);
  }
}

void SiteEngagementMetrics::RecordEngagement(EngagementType type) {
  UMA_HISTOGRAM_ENUMERATION(kEngagementTypeHistogram, type,
                            EngagementType::kLast);
}

}  // namespace site_engagement
