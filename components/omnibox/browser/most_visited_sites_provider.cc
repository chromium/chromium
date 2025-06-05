// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/most_visited_sites_provider.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/top_sites.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/common/url_constants.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"

namespace {

constexpr const int kMaxRecordedTileIndex = 15;
constexpr const size_t kMaxDesktopMostVisitedSuggestions = 10;

constexpr char kHistogramTileTypeCountSearch[] =
    "Omnibox.SuggestTiles.TileTypeCount.Search";
constexpr char kHistogramTileTypeCountURL[] =
    "Omnibox.SuggestTiles.TileTypeCount.URL";
constexpr char kHistogramDeletedTileType[] =
    "Omnibox.SuggestTiles.DeletedTileType";
constexpr char kHistogramDeletedTileIndex[] =
    "Omnibox.SuggestTiles.DeletedTileIndex";
constexpr char kHistogramQueryMostVisitedURLsNumRequested[] =
    "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.NumRequested";
constexpr char kHistogramQueryMostVisitedURLsNumReceived[] =
    "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.NumReceived";
constexpr char kHistogramQueryMostVisitedURLsDuration[] =
    "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.Duration";

constexpr auto kMostVisitedBlocklist =
    base::MakeFixedFlatSet<std::string_view>({
        "accounts.google.com",
    });

bool IsURLBlocklisted(GURL url) {
  // It's fine to block invalid URL's.
  if (!url.is_valid()) {
    return true;
  }

  // Ignore if host contains blocked host name.
  for (const auto& blocked_host : kMostVisitedBlocklist) {
    if (base::EndsWith(url.host(), blocked_host,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }

  return false;
}

GURL StripURL(AutocompleteProviderClient* client,
              const GURL& url,
              const GURL::Replacements& replacements) {
  return AutocompleteMatch::GURLToStrippedGURL(
      url.ReplaceComponents(replacements), AutocompleteInput(),
      client->GetTemplateURLService(), std::u16string(),
      /*keep_search_intent_params=*/false);
}

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

// Creates matches for the desktop implementation.
bool BuildAutocompleteMatches(AutocompleteProvider* provider,
                              AutocompleteProviderClient* client,
                              const history::MostVisitedURLList& urls,
                              ACMatches& matches,
                              const AutocompleteInput& input) {
  if (urls.empty()) {
    return false;
  }

  // Sets to ensure uniqueness of titles and urls for returned suggestions.
  std::unordered_set<std::u16string> match_titles;
  std::unordered_set<std::string> match_urls;

  const TabMatcher& tab_matcher = client->GetTabMatcher();
  // Explicitly clear the query since this isn't done in the
  // `GURLToStrippedGURL()`.
  GURL::Replacements replacements;
  replacements.ClearQuery();

  TemplateURLService* const url_service = client->GetTemplateURLService();
  int relevance =
      omnibox::IsSearchResultsPage(input.current_page_classification())
          ? omnibox::kMostVisitedTilesZeroSuggestLowRelevance
          : omnibox::kMostVisitedTilesZeroSuggestHighRelevance;
  // Store open tab titles and stripped urls to compare to history results.
  std::unordered_set<std::u16string> tab_titles;
  std::unordered_set<std::string> tab_stripped_urls;
  std::vector<TabMatcher::TabWrapper> open_tabs =
      tab_matcher.GetOpenTabs(&input, /*exclude_active_tab=*/false);
  // Deduplication is not guaranteed when the number of open tabs is
  // >= kMaxRequestedURLsFromHistory. This rare case typically occurs
  // when all URLs returned from history are already open. Duplicates
  // may appear, as only the first kMaxRequestedURLsFromHistory tabs
  // are considered during deduplication.
  const size_t limit =
      std::min(open_tabs.size(),
               omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get()
                   .max_requested_urls_from_history);
  for (size_t i = 0; i < limit; ++i) {
    const TabMatcher::TabWrapper& tab = open_tabs[i];
    tab_titles.insert(tab.title);
    tab_stripped_urls.insert((StripURL(client, tab.url, replacements).spec()));
  }

  size_t num_of_matches = 0;
  for (const auto& url : urls) {
    GURL stripped_url = StripURL(client, url.url, replacements);
    // Skip the match if the following is true:
    // - It is an SRP result from DSP
    // - A tab already exists with the same title or stripped URL
    // - Match with the same title already exists
    // - Match with the same stripped url already exists
    if (url_service->IsSearchResultsPageFromDefaultSearchProvider(url.url) ||
        tab_titles.contains(url.title) ||
        tab_stripped_urls.contains(stripped_url.spec()) ||
        IsURLBlocklisted(url.url) || match_titles.contains(url.title) ||
        match_urls.contains(stripped_url.spec())) {
      continue;
    }
    auto match = BuildMatch(provider, client, url.title, url.url, relevance,
                            AutocompleteMatchType::TILE_MOST_VISITED_SITE);
    // Override suggestion group id for desktop most visited matches.
    match.suggestion_group_id = omnibox::GROUP_MOST_VISITED;
    match.subtypes.emplace(omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS);
    match.subtypes.emplace(omnibox::SUBTYPE_URL_BASED);
    matches.emplace_back(std::move(match));
    match_titles.insert(url.title);
    match_urls.insert(stripped_url.spec());
    num_of_matches++;
    --relevance;

    // If there are kMaxDesktopMostVisitedSuggestions valid suggestions, don't
    // keep iterating through all history URL's. This should be enough until
    // the next request.
    if (num_of_matches == kMaxDesktopMostVisitedSuggestions) {
      break;
    }
  }
  return true;
}

// Creates matches for the mobile implementation.
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
    int relevance = omnibox::kMostVisitedTilesZeroSuggestHighRelevance;
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

      --relevance;
    }
  } else {
    AutocompleteMatch match =
        BuildMatch(provider, client, std::u16string(), GURL(),
                   omnibox::kMostVisitedTilesZeroSuggestHighRelevance,
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
  Stop(AutocompleteStopReason::kClobbered);
  if (!AllowMostVisitedSitesSuggestions(client_, input)) {
    return;
  }

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
  auto url_suggestions_on_focus_config =
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get();
  if (url_suggestions_on_focus_config.enabled &&
      url_suggestions_on_focus_config.directly_query_history_service) {
    bool prefetching_enabled =
        url_suggestions_on_focus_config.MostVisitedPrefetchingEnabled();
    CHECK(prefetching_enabled || cached_sites_.empty());
    // Used cached sites if prefetching is enabled.
    if (prefetching_enabled) {
      OnMostVisitedUrlsAvailable(input, cached_sites_);
    }
    // Queries the HistoryService for sites. If prefetching is enabled, this
    // updates `cached_sites_`, otherwise this updates the provider's matches.
    // Prefetching doesn't update the provider's matches since it is expected
    // to only return synchronous results. `debouncer_` used base::Unretained
    // since it does not live beyond the scope of MostVisitedSitesProvider.
    debouncer_->RequestRun(base::BindOnce(
        &MostVisitedSitesProvider::RequestSitesFromHistoryService,
        base::Unretained(this), input));
  } else {
    // TopSites updates itself after a delay. To ensure up-to-date results,
    // force an update now.
    top_sites->SyncWithHistory();
    top_sites->GetMostVisitedURLs(base::BindRepeating(
        &MostVisitedSitesProvider::OnMostVisitedUrlsAvailable,
        request_weak_ptr_factory_.GetWeakPtr(), input));
  }
}

void MostVisitedSitesProvider::StartPrefetch(const AutocompleteInput& input) {
  AutocompleteProvider::StartPrefetch(input);

  TRACE_EVENT0("omnibox", "MostVisitedProvider::StartPrefetch");

  if (!AllowMostVisitedSitesSuggestions(client_, input)) {
    return;
  }

  if (omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get()
          .MostVisitedPrefetchingEnabled()) {
    debouncer_->RequestRun(base::BindOnce(
        &MostVisitedSitesProvider::RequestSitesFromHistoryService,
        base::Unretained(this), input));
  }
}

void MostVisitedSitesProvider::Stop(AutocompleteStopReason stop_reason) {
  AutocompleteProvider::Stop(stop_reason);
  request_weak_ptr_factory_.InvalidateWeakPtrs();
  cancelable_task_tracker_.TryCancelAll();
  debouncer_->CancelRequest();
}

MostVisitedSitesProvider::MostVisitedSitesProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(TYPE_MOST_VISITED_SITES),
      device_form_factor_{ui::GetDeviceFormFactor()},
      client_{client} {
  AddListener(listener);

  auto url_suggestions_on_focus_config =
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get();
  int debounce_delay =
      url_suggestions_on_focus_config.MostVisitedPrefetchingEnabled()
          ? url_suggestions_on_focus_config.prefetch_most_visited_sites_delay_ms
          : 0;
  debouncer_ = std::make_unique<AutocompleteProviderDebouncer>(
      /*from_last_run=*/true, debounce_delay);

  // TopSites updates itself after a delay. To ensure up-to-date results,
  // force an update now.
  scoped_refptr<history::TopSites> top_sites = client_->GetTopSites();
  if (top_sites) {
    top_sites->SyncWithHistory();
  }
}

MostVisitedSitesProvider::~MostVisitedSitesProvider() = default;

void MostVisitedSitesProvider::OnMostVisitedUrlsAvailable(
    AutocompleteInput input,
    const history::MostVisitedURLList& urls) {
  done_ = true;
  if (omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get().enabled) {
    if (BuildAutocompleteMatches(this, client_, urls, matches_, input)) {
      NotifyListeners(true);
    }
  } else if (BuildTileSuggest(this, client_, device_form_factor_, urls,
                              matches_)) {
    NotifyListeners(true);
  }
}

void MostVisitedSitesProvider::OnMostVisitedUrlsFromHistoryServiceAvailable(
    AutocompleteInput input,
    base::ElapsedTimer query_timer,
    history::MostVisitedURLList sites) {
  // Record history query duration and number of results received from
  //`QueryMostVisitedURLs()`.
  base::UmaHistogramTimes(kHistogramQueryMostVisitedURLsDuration,
                          query_timer.Elapsed());
  base::UmaHistogramCounts1000(kHistogramQueryMostVisitedURLsNumReceived,
                               sites.size());

  if (omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get()
          .MostVisitedPrefetchingEnabled()) {
    UpdateCachedSites(sites);
  } else {
    OnMostVisitedUrlsAvailable(input, sites);
  }
}

// static
bool MostVisitedSitesProvider::AllowMostVisitedSitesSuggestions(
    const AutocompleteProviderClient* client,
    const AutocompleteInput& input) {
  const auto& page_url = input.current_url();
  const auto page_class = input.current_page_classification();
  const auto input_type = input.type();

  if (!input.IsZeroSuggest()) {
    return false;
  }

  if (client->IsOffTheRecord()) {
    return false;
  }

  // Check whether current context is one that supports MV tiles.
  // Any context other than those listed below will be rejected.
  if (!omnibox::SupportsMostVisitedSites(page_class)) {
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
          client->GetEmbedderRepresentationOfAboutScheme())))) {
    return false;
  }

  if (omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get().enabled &&
      page_url.scheme() == content::kChromeUIScheme) {
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
         match.type == AutocompleteMatchType::TILE_REPEATABLE_QUERY ||
         match.type == AutocompleteMatchType::HISTORY_URL);

  if (omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get().enabled) {
    history::HistoryService* const history_service =
        client_->GetHistoryService();
    // Delete the underlying URL along with all its visits from the history DB.
    // The resulting HISTORY_URLS_DELETED notification will also cause all
    // caches and indices to drop any data they might have stored pertaining to
    // the URL.
    DCHECK(history_service);
    DCHECK(match.destination_url.is_valid());
    history_service->DeleteURLs({match.destination_url});

    // Delete site from cache if prefetching is enabled.
    cached_sites_.erase(
        std::remove_if(cached_sites_.begin(), cached_sites_.end(),
                       [&match](const history::MostVisitedURL& site) {
                         return site.url == match.destination_url;
                       }),
        cached_sites_.end());
  } else {
    BlockURL(match.destination_url);
  }

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

history::MostVisitedURLList MostVisitedSitesProvider::GetCachedSitesForTesting()
    const {
  return cached_sites_;
}

void MostVisitedSitesProvider::RequestSitesFromHistoryService(
    const AutocompleteInput& input) {
  auto url_suggestions_on_focus_config =
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get();

  size_t requested_result_size = GetRequestedResultSize(input);
  base::UmaHistogramCounts1000(kHistogramQueryMostVisitedURLsNumRequested,
                               requested_result_size);

  base::ElapsedTimer query_timer;
  QueryMostVisitedURLsCallback callback = base::BindOnce(
      &MostVisitedSitesProvider::OnMostVisitedUrlsFromHistoryServiceAvailable,
      request_weak_ptr_factory_.GetWeakPtr(), input, std::move(query_timer));

  client_->GetHistoryService()->QueryMostVisitedURLs(
      requested_result_size, std::move(callback), &cancelable_task_tracker_,
      url_suggestions_on_focus_config.most_visited_recency_factor,
      url_suggestions_on_focus_config.most_visited_recency_window);
}

void MostVisitedSitesProvider::UpdateCachedSites(
    history::MostVisitedURLList sites) {
  cached_sites_ = std::move(sites);
}

size_t MostVisitedSitesProvider::GetRequestedResultSize(
    const AutocompleteInput& input) const {
  const TabMatcher& tab_matcher = client_->GetTabMatcher();

  // The requested results size is the maximum amount of suggestions
  // that can be shown in the omnibox in addition to the number of open tabs
  // and blocklisted sites. Add 1 to `GetOpenTabs` since it doesn't consider
  // the currently active tab.
  return std::min(omnibox_feature_configs::OmniboxZpsSuggestionLimit::Get()
                          .max_suggestions +
                      (tab_matcher.GetOpenTabs(&input).size() + 1) +
                      kMostVisitedBlocklist.size(),
                  omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get()
                      .max_requested_urls_from_history);
}
