// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_sensitivity_cache.h"

#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"

TabSensitivityCache::TabSensitivityCache(Profile* profile) : profile_(profile) {
  page_content_annotations::PageContentAnnotationsService* const
      annotations_service =
          PageContentAnnotationsServiceFactory::GetForProfile(profile);
  if (annotations_service) {
    annotations_service->AddObserver(
        page_content_annotations::AnnotationType::kContentVisibility, this);
  }
}

TabSensitivityCache::~TabSensitivityCache() = default;

std::optional<float> TabSensitivityCache::GetScore(const GURL& url) const {
  const auto it = sensitivy_scores_.find(url);
  if (it == sensitivy_scores_.end()) {
    return std::nullopt;
  }

  return it->second;
}

void TabSensitivityCache::OnPageContentAnnotated(
    const page_content_annotations::HistoryVisit& visit,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  CHECK(result.GetType() ==
        page_content_annotations::AnnotationType::kContentVisibility);

  // Invert the visibility score to get a sensitivity score.
  sensitivy_scores_[visit.url] = 1.0 - result.GetContentVisibilityScore();
  MaybeTrimCacheKeys();
}

void TabSensitivityCache::MaybeTrimCacheKeys() {
  size_t num_tabs_in_profile = 0;
  ProfileBrowserCollection::GetForProfile(profile_)
      ->ForEach([&num_tabs_in_profile](BrowserWindowInterface* browser) {
        num_tabs_in_profile += browser->GetTabStripModel()->count();
        return true;
      });

  // Early return unless we'd remove at least half of the keys.
  if (sensitivy_scores_.size() <= 2 * num_tabs_in_profile) {
    return;
  }

  std::unordered_set<GURL, GURLHash> open_urls;
  ProfileBrowserCollection::GetForProfile(profile_)
      ->ForEach([&open_urls](BrowserWindowInterface* browser) {
        const int num_tabs = browser->GetTabStripModel()->count();
        for (int i = 0; i < num_tabs; i++) {
          open_urls.insert(browser->GetTabStripModel()
                               ->GetWebContentsAt(i)
                               ->GetLastCommittedURL());
        }
        return true;
      });

  std::erase_if(sensitivy_scores_, [&open_urls](auto& key_and_score) {
    return !open_urls.contains(key_and_score.first);
  });
}
