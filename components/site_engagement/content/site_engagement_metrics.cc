// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_engagement/content/site_engagement_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_score.h"

namespace site_engagement {

namespace {

// These numbers are used as suffixes for the
// SiteEngagementService.EngagementScoreBucket_* histogram. If these bases
// change, the EngagementScoreBuckets suffix in histograms.xml should be
// updated.
const int kEngagementBucketHistogramBuckets[] = {0,  10, 20, 30, 40, 50,
                                                 60, 70, 80, 90, 100};

}  // namespace

const char SiteEngagementMetrics::kTotalEngagementHistogram[] =
    "SiteEngagementService.TotalEngagement";

const char SiteEngagementMetrics::kTotalOriginsHistogram[] =
    "SiteEngagementService.OriginsEngaged";

const char SiteEngagementMetrics::kMeanEngagementHistogram[] =
    "SiteEngagementService.MeanEngagement";

const char SiteEngagementMetrics::kMedianEngagementHistogram[] =
    "SiteEngagementService.MedianEngagement";

const char SiteEngagementMetrics::kEngagementScoreHistogram[] =
    "SiteEngagementService.EngagementScore";

const char SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram[] =
    "SiteEngagementService.OriginsWithMaxEngagement";

const char SiteEngagementMetrics::kEngagementTypeHistogram[] =
    "SiteEngagementService.EngagementType";

const char SiteEngagementMetrics::kEngagementBucketHistogramBase[] =
    "SiteEngagementService.EngagementScoreBucket_";

const char SiteEngagementMetrics::kDaysSinceLastShortcutLaunchHistogram[] =
    "SiteEngagementService.DaysSinceLastShortcutLaunch";

void SiteEngagementMetrics::RecordTotalSiteEngagement(double total_engagement) {
  UMA_HISTOGRAM_COUNTS_10000(kTotalEngagementHistogram, total_engagement);
}

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

  std::map<int, int> score_buckets;
  for (size_t i = 0; i < std::size(kEngagementBucketHistogramBuckets); ++i)
    score_buckets[kEngagementBucketHistogramBuckets[i]] = 0;

  for (const auto& detail : details) {
    double score = detail.total_score;
    UMA_HISTOGRAM_COUNTS_100(kEngagementScoreHistogram, score);

    auto bucket = score_buckets.lower_bound(score);
    if (bucket == score_buckets.end())
      continue;

    bucket->second++;
  }

  for (const auto& b : score_buckets) {
    std::string histogram_name =
        kEngagementBucketHistogramBase + base::NumberToString(b.first);

    base::LinearHistogram::FactoryGet(
        histogram_name, 1, 100, 10,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(b.second * 100 / details.size());
  }
}

void SiteEngagementMetrics::RecordOriginsWithMaxEngagement(int total_origins) {
  UMA_HISTOGRAM_COUNTS_100(kOriginsWithMaxEngagementHistogram, total_origins);
}

void SiteEngagementMetrics::RecordEngagement(EngagementType type) {
  UMA_HISTOGRAM_ENUMERATION(kEngagementTypeHistogram, type,
                            EngagementType::kLast);
}

void SiteEngagementMetrics::RecordDaysSinceLastShortcutLaunch(int days) {
  UMA_HISTOGRAM_COUNTS_100(kDaysSinceLastShortcutLaunchHistogram, days);
}

// static
std::vector<std::string>
SiteEngagementMetrics::GetEngagementBucketHistogramNames() {
  std::vector<std::string> histogram_names;
  for (size_t i = 0; i < std::size(kEngagementBucketHistogramBuckets); ++i) {
    histogram_names.push_back(
        kEngagementBucketHistogramBase +
        base::NumberToString(kEngagementBucketHistogramBuckets[i]));
  }

  return histogram_names;
}

}  // namespace site_engagement
