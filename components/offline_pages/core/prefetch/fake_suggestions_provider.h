// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_FAKE_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_FAKE_SUGGESTIONS_PROVIDER_H_

#include <vector>
#include "components/offline_pages/core/prefetch/suggestions_provider.h"

namespace offline_pages {

// A fake implementation of SuggestionsProvider for testing.
class FakeSuggestionsProvider : public SuggestionsProvider {
 public:
  FakeSuggestionsProvider();
  virtual ~FakeSuggestionsProvider();

  // Test methods.
  void SetSuggestions(std::vector<PrefetchSuggestion> suggestions);
  int report_article_list_viewed_count() {
    return report_article_list_viewed_count_;
  }
  std::vector<GURL> article_views() { return article_views_; }
  void ClearViews();

  // SuggestionsProvider implementation.
  void GetCurrentArticleSuggestions(
      SuggestionCallback suggestions_callback) override;
  void ReportArticleListViewed() override;
  void ReportArticleViewed(GURL article_url) override;

 private:
  std::vector<PrefetchSuggestion> suggestions_;
  int report_article_list_viewed_count_ = 0;
  std::vector<GURL> article_views_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_FAKE_SUGGESTIONS_PROVIDER_H_
