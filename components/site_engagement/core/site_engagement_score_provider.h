// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_ENGAGEMENT_CORE_SITE_ENGAGEMENT_SCORE_PROVIDER_H_
#define COMPONENTS_SITE_ENGAGEMENT_CORE_SITE_ENGAGEMENT_SCORE_PROVIDER_H_

class GURL;

namespace site_engagement {

class SiteEngagementScoreProvider {
 public:
  // Returns a non-negative integer representing the engagement score of the
  // origin for this URL.
  virtual double GetScore(const GURL& url) const = 0;

  // Returns the sum of engagement points awarded to all sites.
  virtual double GetTotalEngagementPoints() const = 0;

 protected:
  SiteEngagementScoreProvider() = default;
  virtual ~SiteEngagementScoreProvider() = default;
};

}  // namespace site_engagement

#endif  // COMPONENTS_SITE_ENGAGEMENT_CORE_SITE_ENGAGEMENT_SCORE_PROVIDER_H_
