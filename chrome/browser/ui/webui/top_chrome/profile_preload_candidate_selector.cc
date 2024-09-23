// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/profile_preload_candidate_selector.h"

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/top_chrome/per_profile_webui_tracker.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "url/gurl.h"

using site_engagement::SiteEngagementService;

namespace webui {

ProfilePreloadCandidateSelector::ProfilePreloadCandidateSelector(
    PerProfileWebUITracker* webui_tracker)
    : webui_tracker_(webui_tracker) {}

ProfilePreloadCandidateSelector::~ProfilePreloadCandidateSelector() = default;

void ProfilePreloadCandidateSelector::Init(
    const std::vector<GURL>& preloadable_urls) {
  preloadable_urls_ = preloadable_urls;
}

std::optional<GURL> ProfilePreloadCandidateSelector::GetURLToPreload(
    const PreloadContext& context) const {
  if (preloadable_urls_.empty()) {
    return std::nullopt;
  }

  // Sanity check that the context is a Profile.
  CHECK(context.IsProfile());
  Profile* profile = const_cast<Profile*>(context.GetProfile());
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);

  // Iterate over all preloable URLs to find the URL with the highest
  // engagement score and is not present under the profile.
  // This is linear complexity which is good enough under the assumption that
  // the number of preloadable Top Chrome WebUIs is small. We might revisit
  // the algorithm when necessary.
  std::optional<GURL> result_url;
  double highest_engagement_score = -1.;
  for (const GURL& url : preloadable_urls_) {
    // Skip URLs that are present under the profile.
    if (webui_tracker_->ProfileHasWebUI(profile, url.spec())) {
      continue;
    }

    // Skip URLs that have low engagement score.
    blink::mojom::EngagementLevel engagement_level =
        engagement_service->GetEngagementLevel(url);
    if (engagement_level == blink::mojom::EngagementLevel::NONE ||
        engagement_level == blink::mojom::EngagementLevel::MINIMAL) {
      continue;
    }

    // Update the result if we find a URL with higher score.
    double engagement_score = engagement_service->GetScore(url);
    if (engagement_score > highest_engagement_score) {
      highest_engagement_score = engagement_score;
      result_url = url;
    }
  }

  return result_url;
}

}  // namespace webui
