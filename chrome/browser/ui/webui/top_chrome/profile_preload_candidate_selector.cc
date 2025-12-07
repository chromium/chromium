// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/profile_preload_candidate_selector.h"

#include <vector>

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/top_chrome/per_profile_webui_tracker.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "url/gurl.h"

using site_engagement::SiteEngagementService;

namespace webui {

ProfilePreloadCandidateSelector::ProfilePreloadCandidateSelector(
    PerProfileWebUITracker* webui_tracker)
    : webui_tracker_(webui_tracker) {
  no_preload_if_most_engaged_url_is_background_ = base::FeatureList::IsEnabled(
      features::kPreloadTopChromeWebUILessNavigations);
}

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

  // Sort the URLs by engagement score in descending order. This is O(nlogn)
  // complexity which is good enough under the assumption that the number of
  // preloadable Top Chrome WebUIs is small. We might revisit the algorithm when
  // necessary.
  std::vector<GURL> sorted_urls = preloadable_urls_;
  std::sort(sorted_urls.begin(), sorted_urls.end(),
            [&](const GURL& a, const GURL& b) {
              return engagement_service->GetScore(a) >
                     engagement_service->GetScore(b);
            });

  // Iterate over all preloable URLs to find the URL with the highest
  // engagement score and is not present under the profile.
  for (const GURL& url : sorted_urls) {
    // Skip if this URL is disabled. This is seen in History Clusters side panel
    // where some locales are not supported.
    auto* config = TopChromeWebUIConfig::From(profile, url);
    if (!config || !config->IsWebUIEnabled(profile)) {
      continue;
    }

    // Skip URLs that have low engagement score. Because the URLs are sorted by
    // engagement score, we can stop iterating once we find a URL with low
    // engagement score.
    blink::mojom::EngagementLevel engagement_level =
        engagement_service->GetEngagementLevel(url);
    if (engagement_level == blink::mojom::EngagementLevel::NONE ||
        engagement_level == blink::mojom::EngagementLevel::MINIMAL) {
      return std::nullopt;
    }

    if (IsUrlExcludedByFlag(url)) {
      continue;
    }

    // If the URL is in the background, don't preload at all.
    if (no_preload_if_most_engaged_url_is_background_ &&
         webui_tracker_->ProfileHasBackgroundWebUI(profile, url.spec())) {
      return std::nullopt;
    }

    // Skip URLs that are already present under the profile.
    if (webui_tracker_->ProfileHasWebUI(profile, url.spec())) {
      continue;
    }

    // We have found a candidate URL with the highest engagement score.
    return url;
  }

  return std::nullopt;
}

}  // namespace webui
