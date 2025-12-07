// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FEATURED_SEARCH_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_FEATURED_SEARCH_PROVIDER_H_

#include <stddef.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/search_engines/template_url_service.h"

class AutocompleteInput;
class AutocompleteProviderClient;
class TemplateURLService;
class AutocompleteResult;

// This is the provider for built-in URLs, such as about:settings and
// chrome://version, as well as the built-in Starter Pack search engines.
class FeaturedSearchProvider : public AutocompleteProvider {
 public:
  FeaturedSearchProvider(AutocompleteProviderClient* client,
                         bool show_iph_matches);
  FeaturedSearchProvider(const FeaturedSearchProvider&) = delete;
  FeaturedSearchProvider& operator=(const FeaturedSearchProvider&) = delete;

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void DeleteMatch(const AutocompleteMatch& match) override;

  // Called by `AutocompleteController` after ranking has settled. Increments
  // IPH shown counts.
  void RegisterDisplayedMatches(const AutocompleteResult& result);

 private:
  ~FeaturedSearchProvider() override;

  // Populates `matches_` with matching starter pack keywords such as @history,
  // and @bookmarks
  void AddFeaturedKeywordMatches(const AutocompleteInput& input);

  // Constructs an AutocompleteMatch for starter pack suggestions such as
  // @bookmarks, @history, etc. and adds it to `matches_`.
  void AddStarterPackMatch(const TemplateURL& template_url,
                           const AutocompleteInput& input);

  // Constructs a NULL_RESULT_MESSAGE match that is informational only and
  // cannot be acted upon.  This match delivers an IPH message directing users
  // to the starter pack feature.
  void AddIPHMatch(IphType iph_type,
                   const std::u16string& iph_contents,
                   const std::u16string& matched_term,
                   const std::u16string& iph_link_text,
                   const GURL& iph_link_url,
                   int relevance,
                   bool deletable);

  void AddFeaturedEnterpriseSearchMatch(const TemplateURL& template_url,
                                        const AutocompleteInput& input);

  // Returns whether to show the IPH match for `iph_type`.
  bool ShouldShowIPH(IphType iph_type) const;

  // Whether to show the @gemini keyword promo row in zero-state.
  bool ShouldShowGeminiIPHMatch() const;
  void AddGeminiIPHMatch();

  // Whether to show the Enterprise Search Aggregator keyword promo row
  // in zero-state.
  bool ShouldShowEnterpriseSearchAggregatorIPHMatch() const;
  void AddEnterpriseSearchAggregatorIPHMatch();

  // Whether to show the Featured Enterprise Site Search keyword promo row in
  // zero-state.
  bool ShouldShowFeaturedEnterpriseSiteSearchIPHMatch() const;
  void AddFeaturedEnterpriseSiteSearchIPHMatch();

  // Whether to show the History Embeddings promo row in @history scope.
  bool ShouldShowHistoryEmbeddingsSettingsPromoIphMatch() const;
  void AddHistoryEmbeddingsSettingsPromoIphMatch();

  // Whether to show the History Embeddings disclaimer row in @history scope.
  bool ShouldShowHistoryEmbeddingsDisclaimerIphMatch() const;
  void AddHistoryEmbeddingsDisclaimerIphMatch();

  // Whether to show the @history keyword promo row in zero-state.
  bool ShouldShowHistoryScopePromoIphMatch() const;
  void AddHistoryScopePromoIphMatch();

  // Whether to show the @history (embeddings) keyword promo row in zero-state.
  bool ShouldShowHistoryEmbeddingsScopePromoIphMatch() const;
  void AddHistoryEmbeddingsScopePromoIphMatch();

  raw_ptr<AutocompleteProviderClient> client_;
  raw_ptr<TemplateURLService> template_url_service_;
  const bool show_iph_matches_;

  // The number of times the IPH row has been shown so far in this browser
  // session. Shared by all IPH types. Reset when, e.g., the user opens a new
  // browser window.
  size_t iph_shown_in_browser_session_count_{0};

  // Whether an IPH match was shown during the current omnibox session. Used to
  // avoid incrementing `iph_shown_in_browser_session_count_` more than once per
  // omnibox session. Reset when, e.g.,  the user refocuses the omnibox.
  // omnibox.
  bool iph_shown_in_omnibox_session_ = false;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FEATURED_SEARCH_PROVIDER_H_
