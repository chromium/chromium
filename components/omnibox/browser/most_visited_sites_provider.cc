// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/most_visited_sites_provider.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "components/history/core/browser/top_sites.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"

namespace {
// The relevance score for navsuggest tiles.
// Navsuggest tiles should be positioned below the Query Tiles object.
constexpr const int kMostVisitedTilesRelevance = 1500;
}  // namespace

void MostVisitedSitesProvider::Start(const AutocompleteInput& input,
                                     bool minimal_changes) {
  Stop(true, false);
  if (!AllowMostVisitedSitesSuggestions(input))
    return;

  scoped_refptr<history::TopSites> top_sites = client_->GetTopSites();
  if (!top_sites)
    return;

  top_sites->GetMostVisitedURLs(
      base::BindRepeating(&MostVisitedSitesProvider::OnMostVisitedUrlsAvailable,
                          request_weak_ptr_factory_.GetWeakPtr()));
}

void MostVisitedSitesProvider::Stop(bool clear_cached_results,
                                    bool due_to_user_inactivity) {
  request_weak_ptr_factory_.InvalidateWeakPtrs();
  matches_.clear();
}

MostVisitedSitesProvider::MostVisitedSitesProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(TYPE_MOST_VISITED_SITES),
      client_{client},
      listener_{listener} {}

MostVisitedSitesProvider::~MostVisitedSitesProvider() = default;

AutocompleteMatch MostVisitedSitesProvider::BuildMatch(
    const base::string16& description,
    const GURL& url,
    int relevance,
    AutocompleteMatchType::Type type) {
  AutocompleteMatch match(this, relevance, false, type);
  match.destination_url = url;

  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, url_formatter::FormatUrl(url), client_->GetSchemeClassifier(),
          nullptr);

  // Zero suggest results should always omit protocols and never appear bold.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, false);
  match.contents = url_formatter::FormatUrl(
      url, format_types, net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  match.contents_class = ClassifyTermMatches({}, match.contents.length(), 0,
                                             ACMatchClassification::URL);

  match.description = AutocompleteMatch::SanitizeString(description);
  match.description_class = ClassifyTermMatches({}, match.description.length(),
                                                0, ACMatchClassification::NONE);

  return match;
}

void MostVisitedSitesProvider::OnMostVisitedUrlsAvailable(
    const history::MostVisitedURLList& urls) {
  if (urls.empty())
    return;

  if (base::FeatureList::IsEnabled(omnibox::kMostVisitedTiles)) {
    AutocompleteMatch match = BuildMatch(
        base::string16(), GURL::EmptyGURL(), kMostVisitedTilesRelevance,
        AutocompleteMatchType::TILE_NAVSUGGEST);
    match.navsuggest_tiles.reserve(urls.size());

    for (const auto& url : urls) {
      match.navsuggest_tiles.push_back({url.url, url.title});
    }
    matches_.push_back(std::move(match));
  } else {
    int relevance = 600;
    for (const auto& url : urls) {
      matches_.emplace_back(BuildMatch(url.title, url.url, relevance,
                                       AutocompleteMatchType::NAVSUGGEST));
      --relevance;
    }
  }
  listener_->OnProviderUpdate(true);
}

bool MostVisitedSitesProvider::AllowMostVisitedSitesSuggestions(
    const AutocompleteInput& input) const {
  const auto& page_url = input.current_url();
  const auto page_class = input.current_page_classification();
  const auto input_type = input.type();

  if (input.focus_type() == OmniboxFocusType::DEFAULT)
    return false;

  if (client_->IsOffTheRecord())
    return false;

  // Only serve Most Visited suggestions when the current context is page visit.
  if (page_class != metrics::OmniboxEventProto::OTHER &&
      page_class != metrics::OmniboxEventProto::ANDROID_SEARCH_WIDGET) {
    return false;
  }

  // When omnibox contains pre-populated content, only show zero suggest for
  // pages with URLs the user will recognize.
  //
  // This list intentionally does not include items such as ftp: and file:
  // because (a) these do not work on Android and iOS, where most visited
  // zero suggest is launched and (b) on desktop, where contextual zero suggest
  // is running, these types of schemes aren't eligible to be sent to the
  // server to ask for suggestions (and thus in practice we won't display zero
  // suggest for them).
  if (input_type != metrics::OmniboxInputType::EMPTY &&
      !(page_url.is_valid() &&
        ((page_url.scheme() == url::kHttpScheme) ||
         (page_url.scheme() == url::kHttpsScheme) ||
         (page_url.scheme() == url::kAboutScheme) ||
         (page_url.scheme() ==
          client_->GetEmbedderRepresentationOfAboutScheme())))) {
    return false;
  }

  return true;
}
