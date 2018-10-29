// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_SUGGESTIONS_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "url/gurl.h"

namespace offline_pages {

// Data struct for a suggestion of an article to be prefetched.
struct PrefetchSuggestion {
  PrefetchSuggestion();
  PrefetchSuggestion(const PrefetchSuggestion&);
  PrefetchSuggestion(PrefetchSuggestion&&);
  ~PrefetchSuggestion();

  // The URL of the suggested article. It acts as a unique key for the
  // suggestion and may be de-duplicated if the same URL is suggested more than
  // once.
  GURL article_url;
  // The title of the suggested article.
  std::string article_title;
  // The publisher name/web site the article is attributed to.
  std::string article_attribution;
  // A snippet of the article's contents.
  std::string article_snippet;
  // The URL to the thumbnail image representing the suggested article.
  GURL thumbnail_url;
  // The URL to the favicon image of the article's hosting web site.
  GURL favicon_url;
};

// Interface implemented by the suggestions provider.
class SuggestionsProvider {
 public:
  using SuggestionCallback =
      base::OnceCallback<void(std::vector<PrefetchSuggestion>)>;

  // Request the list of current article suggestions, to be returned via the
  // provided callback (via PostTask) in descending priority order. Freshest
  // articles are prefetched first based both on the order they are listed and
  // on the timestamp at which the suggestion was last seen.
  virtual void GetCurrentArticleSuggestions(
      SuggestionCallback suggestions_callback) = 0;

  // Notifies that a non-empty list of prefetched articles was presented to the
  // user.
  virtual void ReportArticleListViewed() = 0;

  // Notifies that the a specific prefetched article was presented to the user.
  // This will always provide the original suggested URL, not the potentially
  // different downloaded one in case redirects take place during archive
  // generation.
  virtual void ReportArticleViewed(GURL article_url) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_SUGGESTIONS_PROVIDER_H_
