// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/most_visited_sites_provider.h"

#include <string>
#include <vector>

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
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"

namespace {
// The relevance score for suggest tiles represented as a single tiling match.
// Suggest tiles are placed in a dedicated SECTION_MOBILE_MOST_VISITED
// making its relative relevance score not important.
constexpr const int kMostVisitedTilesAggregateRelevance = 1;

// The relevance score for suggest tiles represented as individual matches.
// Repeatable Queries are recognized as searches, and may get merged to higher
// ranking search suggestions listed below the carousel.
constexpr const int kMostVisitedTilesIndividualHighRelevance = 1600;
// Matches known to be off-screen by default are listed as low-relevance.
// If we have additional AutocompleteMatches listed below the MV carousel
// pointing to the same destination, we want the tiles to be deduplicated to
// these matches.
constexpr const int kMostVisitedTilesIndividualLowRelevance = 100;
// Index of the last high-relevance tile.
constexpr const int kLastHighRelevanceIndividualTile = 4;

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
                      ui::DeviceFormFactor device_form_factor,
                      const TileContainer& container,
                      ACMatches& matches) {
  if (container.empty()) {
    base::UmaHistogramExactLinear(kHistogramTileTypeCountSearch, 0,
                                  kMaxRecordedTileIndex);
    base::UmaHistogramExactLinear(kHistogramTileTypeCountURL, 0,
                                  kMaxRecordedTileIndex);
    return false;
  }

  size_t num_search_tiles = 0;
  size_t num_url_tiles = 0;

  if (base::FeatureList::IsEnabled(
          omnibox::kMostVisitedTilesHorizontalRenderGroup)) {
    auto* const url_service = client->GetTemplateURLService();
    auto* const dse = url_service->GetDefaultSearchProvider();
    int relevance = kMostVisitedTilesIndividualHighRelevance;
    for (const auto& tile : container) {
      // TODO(crbug.com/40279214): pass this information from History layer via
      // history::MostVisitedURL.
      bool is_search =
          url_service->IsSearchResultsPageFromDefaultSearchProvider(tile.url);
      auto match =
          BuildMatch(provider, client, tile.title, tile.url, relevance,
                     is_search ? AutocompleteMatchType::TILE_REPEATABLE_QUERY
                               : AutocompleteMatchType::TILE_MOST_VISITED_SITE);
      if (is_search) {
        match.subtypes.emplace(
            omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES);
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

        // Supply blanket SearchTermsArgs so we can also report SearchBoxStats.
        match.search_terms_args =
            std::make_unique<TemplateURLRef::SearchTermsArgs>(query);
        num_search_tiles++;
      } else {
        match.subtypes.emplace(
            omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS);
        match.subtypes.emplace(omnibox::SUBTYPE_URL_BASED);
        num_url_tiles++;
      }
      matches.emplace_back(std::move(match));

      // On phones, we fully expose a fixed number of matches. Matches beyond
      // that number are partially or fully concealed by design. Drop relevance
      // for these matches.
      if (matches.size() == kLastHighRelevanceIndividualTile &&
          device_form_factor == ui::DEVICE_FORM_FACTOR_PHONE) {
        relevance = kMostVisitedTilesIndividualLowRelevance;
      }
      --relevance;
    }
  } else {
    AutocompleteMatch match =
        BuildMatch(provider, client, std::u16string(), GURL(),
                   kMostVisitedTilesAggregateRelevance,
                   AutocompleteMatchType::TILE_NAVSUGGEST);

    match.suggest_tiles.reserve(container.size());
    auto* const url_service = client->GetTemplateURLService();

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

    match.subtypes.emplace(omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS);
    match.subtypes.emplace(omnibox::SUBTYPE_URL_BASED);
    matches.push_back(std::move(match));
  }

  base::UmaHistogramExactLinear(kHistogramTileTypeCountSearch, num_search_tiles,
                                kMaxRecordedTileIndex);
  base::UmaHistogramExactLinear(kHistogramTileTypeCountURL, num_url_tiles,
                                kMaxRecordedTileIndex);

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
    : AutocompleteProvider(TYPE_MOST_VISITED_SITES),
      device_form_factor_{ui::GetDeviceFormFactor()},
      client_{client} {
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
  if (BuildTileSuggest(this, client_, device_form_factor_, urls, matches_)) {
    NotifyListeners(true);
  }
}

bool MostVisitedSitesProvider::AllowMostVisitedSitesSuggestions(
    const AutocompleteInput& input) const {
  const auto& page_url = input.current_url();
  const auto page_class = input.current_page_classification();
  const auto input_type = input.type();

  if (!input.IsZeroSuggest()) {
    return false;
  }

  if (client_->IsOffTheRecord())
    return false;

  // Check whether current context is one that supports MV tiles.
  // Any context other than those listed below will be rejected.
  if (page_class != metrics::OmniboxEventProto::OTHER &&
      page_class != metrics::OmniboxEventProto::ANDROID_SEARCH_WIDGET &&
      page_class != metrics::OmniboxEventProto::ANDROID_SHORTCUTS_WIDGET) {
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
  std::erase_if(tiles_to_update, [&tile_to_delete](const auto& tile) {
    return tile.url == tile_to_delete.url;
  });

  if (tiles_to_update.empty()) {
    matches_.clear();
  }
}
