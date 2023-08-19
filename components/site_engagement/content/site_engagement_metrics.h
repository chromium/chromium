// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_ENGAGEMENT_CONTENT_SITE_ENGAGEMENT_METRICS_H_
#define COMPONENTS_SITE_ENGAGEMENT_CONTENT_SITE_ENGAGEMENT_METRICS_H_

#include "base/gtest_prod_util.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"

namespace site_engagement {

enum class EngagementType;

// Helper class managing the UMA histograms for the Site Engagement Service.
class SiteEngagementMetrics {
 public:
  static void RecordTotalOriginsEngaged(int total_origins);
  static void RecordMeanEngagement(double mean_engagement);
  static void RecordMedianEngagement(double median_engagement);
  static void RecordEngagementScores(
      const std::vector<mojom::SiteEngagementDetails>& details);
  static void RecordEngagement(EngagementType type);

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, CheckHistograms);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           GetTotalNotificationPoints);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, LastShortcutLaunch);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementHelperTest,
                           MixedInputEngagementAccumulation);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementHelperBrowserTest,
                           SiteEngagementHelperInPrerendering);
  static const char kTotalOriginsHistogram[];
  static const char kMeanEngagementHistogram[];
  static const char kMedianEngagementHistogram[];
  static const char kEngagementScoreHistogram[];
  static const char kEngagementTypeHistogram[];
};

}  // namespace site_engagement

#endif  // COMPONENTS_SITE_ENGAGEMENT_CONTENT_SITE_ENGAGEMENT_METRICS_H_
