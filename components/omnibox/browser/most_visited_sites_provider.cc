// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/most_visited_sites_provider.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
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
#include "third_party/omnibox_proto/types.pb.h"
#include "url/gurl.h"

namespace {
// The relevance score for suggest tiles.
// Suggest tiles are placed in a dedicated SECTION_MOBILE_MOST_VISITED
// making its relative relevance score not important.
constexpr const int kMostVisitedTilesRelevance = 1;
constexpr const int kMaxRecordedTileIndex = 15;

constexpr char kHistogramTileTypeCountSearch[] =
    "Omnibox.SuggestTiles.TileTypeCount.Search";
constexpr char kHistogramTileTypeCountURL[] =
    "Omnibox.SuggestTiles.TileTypeCount.URL";
constexpr char kHistogramDeletedTileType[] =
    "Omnibox.SuggestTiles.DeletedTileType";
constexpr char kHistogramDeletedTileIndex[] =
    "Omnibox.SuggestTiles.DeletedTileIndex";

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.omnibox.suggestions.mostvisited)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SuggestTileType
enum SuggestTileType { kOther = 0, kURL = 1, kSearch = 2, kCount = 3 };

// Constructs an AutocompleteMatch from supplied details.
AutocompleteMatch BuildMatch(AutocompleteProvider* provider,
                             AutocompleteProviderClient* client,
                             const std::u16string& description,
                             const GURL& url,
                             int relevance,
                             AutocompleteMatchType::Type type) {
  AutocompleteMatch match(provider, relevance, true, type);
  match.suggest_type = omnibox::TYPE_NAVIGATION;
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

  match.suggestion_group_id = omnibox::GROUP_MOBILE_MOST_VISITED;
  return match;
}

template <typename TileContainer>
bool BuildTileSuggest(AutocompleteProvider* provider,
                      AutocompleteProviderClient* const client,
                      const TileContainer& container,
                      ACMatches& matches) {
  if (container.empty()) {
    base::UmaHistogramExactLinear(kHistogramTileTypeCountSearch, 0,
                                  kMaxRecordedTileIndex);
    base::UmaHistogramExactLinear(kHistogramTileTypeCountURL, 0,
                                  kMaxRecordedTileIndex);
    return false;
  }

  if (base::FeatureList::IsEnabled(
          omnibox::kMostVisitedTilesHorizontalRenderGroup)) {
    auto* const url_service = client->GetTemplateURLService();
    auto* const dse = url_service->GetDefaultSearchProvider();
    int relevance = 100;
    for (const auto& tile : container) {
      // TODO(crbug/1474087): pass this information from History layer via
      // history::MostVisitedURL.
      bool is_search =
          url_service->IsSearchResultsPageFromDefaultSearchProvider(tile.url);
      auto match =
          BuildMatch(provider, client, tile.title, tile.url, relevance,
                     is_search ? AutocompleteMatchType::TILE_REPEATABLE_QUERY
                               : AutocompleteMatchType::TILE_MOST_VISITED_SITE);
      if (is_search) {
        match.keyword = dse->keyword();
        std::u16string query = tile.title;

        if (dse->url_ref().SupportsReplacement(
                url_service->search_terms_data())) {
          dse->ExtractSearchTermsFromURL(
              tile.url, url_service->search_terms_data(), &query);
        }
        match.fill_into_edit = query;
        match.contents = query;
        match.suggest_type = omnibox::TYPE_QUERY;
      }
      matches.emplace_back(std::move(match));
      --relevance;
    }
  } else if (base::FeatureList::IsEnabled(omnibox::kMostVisitedTiles)) {
    AutocompleteMatch match = BuildMatch(
        provider, client, std::u16string(), GURL::EmptyGURL(),
        kMostVisitedTilesRelevance, AutocompleteMatchType::TILE_NAVSUGGEST);

    match.suggest_tiles.reserve(container.size());
    auto* const url_service = client->GetTemplateURLService();

    size_t num_search_tiles = 0;
    size_t num_url_tiles = 0;

    for (const auto& tile : container) {
      bool is_search =
          url_service->IsSearchResultsPageFromDefaultSearchProvider(tile.url);

      match.suggest_tiles.push_back({
          .url = tile.url,
          .title = tile.title,
          .is_search = is_search,
      });

      if (is_search) {
        num_search_tiles++;
      } else {
        num_url_tiles++;
      }
    }

    base::UmaHistogramExactLinear(kHistogramTileTypeCountSearch,
                                  num_search_tiles, kMaxRecordedTileIndex);
    base::UmaHistogramExactLinear(kHistogramTileTypeCountURL, num_url_tiles,
                                  kMaxRecordedTileIndex);

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

  // If TopSites has not yet been loaded, then `OnMostVisitedUrlsAvailable` will
  // be called asynchronously, so we need to first check that async calls are
  // allowed for the given input.
  if (!top_sites->loaded() && input.omit_asynchronous_matches()) {
    return;
  }

  done_ = false;

  // TODO(ender): Relocate this to StartPrefetch() when additional prefetch
  // contexts are available.
  // TopSites updates itself after a delay. To ensure up-to-date results,
  // force an update now.
  top_sites->SyncWithHistory();
  top_sites->GetMostVisitedURLs(
      base::BindRepeating(&MostVisitedSitesProvider::OnMostVisitedUrlsAvailable,
                          request_weak_ptr_factory_.GetWeakPtr()));
}

void MostVisitedSitesProvider::Stop(bool clear_cached_results,
                                    bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);
  request_weak_ptr_factory_.InvalidateWeakPtrs();
}

MostVisitedSitesProvider::MostVisitedSitesProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(TYPE_MOST_VISITED_SITES), client_{client} {
  AddListener(listener);

  // TopSites updates itself after a delay. To ensure up-to-date results,
  // force an update now.
  scoped_refptr<history::TopSites> top_sites = client_->GetTopSites();
  if (top_sites) {
    top_sites->SyncWithHistory();
  }
}

MostVisitedSitesProvider::~MostVisitedSitesProvider() = default;

void MostVisitedSitesProvider::OnMostVisitedUrlsAvailable(
    const history::MostVisitedURLList& urls) {
  done_ = true;
  if (BuildTileSuggest(this, client_, urls, matches_))
    NotifyListeners(true);
}

bool MostVisitedSitesProvider::AllowMostVisitedSitesSuggestions(
    const AutocompleteInput& input) const {
  const auto& page_url = input.current_url();
  const auto page_class = input.current_page_classification();
  const auto input_type = input.type();

  if (input.focus_type() == metrics::OmniboxFocusType::INTERACTION_DEFAULT)
    return false;

  if (client_->IsOffTheRecord())
    return false;

  // This code guards cases when flag is disabled. Upon post-launch cleanup
  // we just delete this
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxMostVisitedTilesOnSrp) &&
      (page_class == metrics::OmniboxEventProto::
                         SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT)) {
    return false;
  }

  // Check whether current context is one that supports MV tiles.
  // Any context other than those listed below will be rejected.
  if (page_class != metrics::OmniboxEventProto::OTHER &&
      page_class != metrics::OmniboxEventProto::ANDROID_SEARCH_WIDGET &&
      page_class != metrics::OmniboxEventProto::ANDROID_SHORTCUTS_WIDGET &&
      page_class != metrics::OmniboxEventProto::
                        SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT) {
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
  DCHECK(match.type == AutocompleteMatchType::NAVSUGGEST ||
         match.type == AutocompleteMatchType::TILE_MOST_VISITED_SITE ||
         match.type == AutocompleteMatchType::TILE_REPEATABLE_QUERY);

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

  const auto& tile_to_delete = source_match.suggest_tiles[element_index];

  base::UmaHistogramExactLinear(kHistogramDeletedTileIndex, element_index,
                                kMaxRecordedTileIndex);
  base::UmaHistogramExactLinear(kHistogramDeletedTileType,
                                tile_to_delete.is_search
                                    ? SuggestTileType::kSearch
                                    : SuggestTileType::kURL,
                                SuggestTileType::kCount);

  BlockURL(tile_to_delete.url);
  auto& tiles_to_update = matches_[0].suggest_tiles;
  base::EraseIf(tiles_to_update, [&tile_to_delete](const auto& tile) {
    return tile.url == tile_to_delete.url;
  });

  if (tiles_to_update.empty()) {
    matches_.clear();
  }
}
