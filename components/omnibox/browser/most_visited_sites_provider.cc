// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/most_visited_sites_provider.h"

#include <string>

#include "base/bind.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/strings/escape.h"
#include "components/history/core/browser/top_sites.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"

namespace {
// The relevance score for suggest tiles.
// Suggest tiles should be positioned below the Query Tiles object.
constexpr const int kMostVisitedTilesRelevance = 1500;

// Constructs an AutocompleteMatch from supplied details.
AutocompleteMatch BuildMatch(AutocompleteProvider* provider,
                             AutocompleteProviderClient* client,
                             const std::u16string& description,
                             const GURL& url,
                             int relevance,
                             AutocompleteMatchType::Type type) {
  AutocompleteMatch match(provider, relevance, true, type);
  match.destination_url = url;

  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, url_formatter::FormatUrl(url), client->GetSchemeClassifier(),
          nullptr);

  // Zero suggest results should always omit protocols and never appear bold.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, false);
  match.contents = url_formatter::FormatUrl(
      url, format_types, base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  match.contents_class = ClassifyTermMatches({}, match.contents.length(), 0,
                                             ACMatchClassification::URL);

  match.description = AutocompleteMatch::SanitizeString(description);
  match.description_class = ClassifyTermMatches({}, match.description.length(),
                                                0, ACMatchClassification::NONE);

  return match;
}

template <typename TileContainer>
bool BuildTileSuggest(AutocompleteProvider* provider,
                      AutocompleteProviderClient* const client,
                      const TileContainer& container,
                      ACMatches& matches) {
  if (container.empty())
    return false;

  if (base::FeatureList::IsEnabled(omnibox::kMostVisitedTiles)) {
    AutocompleteMatch match = BuildMatch(
        provider, client, std::u16string(), GURL::EmptyGURL(),
        kMostVisitedTilesRelevance, AutocompleteMatchType::TILE_NAVSUGGEST);

    match.suggest_tiles.reserve(container.size());
    auto* const url_service = client->GetTemplateURLService();

    for (const auto& tile : container) {
      match.suggest_tiles.push_back({
          .url = tile.url,
          .title = tile.title,
          .is_search =
              url_service->IsSearchResultsPageFromDefaultSearchProvider(
                  tile.url),
      });
    }
    matches.push_back(std::move(match));
  } else {
    int relevance = 600;
    for (const auto& tile : container) {
      matches.emplace_back(BuildMatch(provider, client, tile.title, tile.url,
                                      relevance,
                                      AutocompleteMatchType::NAVSUGGEST));
      --relevance;
    }
  }
  return true;
}

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
  if (clear_cached_results)
    matches_.clear();
}

MostVisitedSitesProvider::MostVisitedSitesProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(TYPE_MOST_VISITED_SITES), client_{client} {
  AddListener(listener);
}

MostVisitedSitesProvider::~MostVisitedSitesProvider() = default;

void MostVisitedSitesProvider::OnMostVisitedUrlsAvailable(
    const history::MostVisitedURLList& urls) {
  if (BuildTileSuggest(this, client_, urls, matches_))
    NotifyListeners(true);
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

  // Check whether current context is one that supports MV tiles.
  // Any context other than those listed below will be rejected.
  if (page_class != metrics::OmniboxEventProto::OTHER &&
      page_class != metrics::OmniboxEventProto::ANDROID_SEARCH_WIDGET &&
      page_class != metrics::OmniboxEventProto::ANDROID_SHORTCUTS_WIDGET &&
      page_class != metrics::OmniboxEventProto::START_SURFACE_HOMEPAGE &&
      page_class != metrics::OmniboxEventProto::START_SURFACE_NEW_TAB) {
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

void MostVisitedSitesProvider::BlockURL(const GURL& site_url) {
  scoped_refptr<history::TopSites> top_sites = client_->GetTopSites();
  if (top_sites) {
    top_sites->AddBlockedUrl(site_url);
  }
}

void MostVisitedSitesProvider::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK_EQ(match.type, AutocompleteMatchType::NAVSUGGEST);

  BlockURL(match.destination_url);
  for (auto i = matches_.begin(); i != matches_.end(); ++i) {
    if (i->contents == match.contents) {
      matches_.erase(i);
      break;
    }
  }
}
void MostVisitedSitesProvider::DeleteMatchElement(
    const AutocompleteMatch& source_match,
    size_t element_index) {
  DCHECK_EQ(source_match.type, AutocompleteMatchType::TILE_NAVSUGGEST);
  DCHECK_GE(element_index, 0u);
  DCHECK_LT((size_t)element_index, source_match.suggest_tiles.size());

  // Attempt to modify the match in place.
  DCHECK_EQ(matches_.size(), 1ul);
  DCHECK_EQ(matches_[0].type, AutocompleteMatchType::TILE_NAVSUGGEST);

  if (source_match.type != AutocompleteMatchType::TILE_NAVSUGGEST ||
      element_index < 0u ||
      element_index >= source_match.suggest_tiles.size() ||
      matches_.size() != 1u ||
      matches_[0].type != AutocompleteMatchType::TILE_NAVSUGGEST) {
    return;
  }

  const auto& url_to_delete = source_match.suggest_tiles[element_index].url;
  BlockURL(url_to_delete);
  auto& tiles_to_update = matches_[0].suggest_tiles;
  base::EraseIf(tiles_to_update, [&url_to_delete](const auto& tile) {
    return tile.url == url_to_delete;
  });

  if (tiles_to_update.empty()) {
    matches_.clear();
  }
}
