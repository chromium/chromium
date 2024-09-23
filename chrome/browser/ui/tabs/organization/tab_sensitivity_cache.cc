// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_sensitivity_cache.h"

#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
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
    const GURL& url,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  CHECK(result.GetType() ==
        page_content_annotations::AnnotationType::kContentVisibility);

  // Invert the visibility score to get a sensitivity score.
  sensitivy_scores_[url] = 1.0 - result.GetContentVisibilityScore();
  MaybeTrimCacheKeys();
}

void TabSensitivityCache::MaybeTrimCacheKeys() {
  const std::vector<Browser*> browsers =
      chrome::FindAllTabbedBrowsersWithProfile(profile_);

  size_t num_tabs_in_profile = 0;
  for (auto* browser : browsers) {
    num_tabs_in_profile += browser->tab_strip_model()->count();
  }

  // Early return unless we'd remove at least half of the keys.
  if (sensitivy_scores_.size() <= 2 * num_tabs_in_profile) {
    return;
  }

  std::unordered_set<GURL, GURLHash> open_urls;
  for (auto* browser : browsers) {
    const int num_tabs = browser->tab_strip_model()->count();
    for (int i = 0; i < num_tabs; i++) {
      open_urls.insert(browser->tab_strip_model()
                           ->GetWebContentsAt(i)
                           ->GetLastCommittedURL());
    }
  }

  std::erase_if(sensitivy_scores_, [&open_urls](auto& key_and_score) {
    return !open_urls.contains(key_and_score.first);
  });
}
