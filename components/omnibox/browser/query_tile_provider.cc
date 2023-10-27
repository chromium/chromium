// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/query_tile_provider.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/query_tiles/tile_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"

// The relevance score for query tile match.
constexpr int kQueryTilesMatchRelevanceScore = 1600;

QueryTileProvider::QueryTileProvider(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_QUERY_TILE),
      client_(client) {
  AddListener(listener);
}

QueryTileProvider::~QueryTileProvider() = default;

void QueryTileProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  Stop(true, false);
  if (!IsAllowedInContext(input)) {
    return;
  }
  done_ = false;

  // Fetch data from server only if our caches are empty.
  if (tiles_.empty() && !input.omit_asynchronous_matches()) {
    client_->GetQueryTileService()->GetQueryTiles(
        base::BindOnce(&QueryTileProvider::OnTilesFetched,
                       weak_ptr_factory_.GetWeakPtr(), /*is_prefetch=*/false));
  } else {
    // Serve results from cache synchronously.
    BuildSuggestions();
  }
}

void QueryTileProvider::StartPrefetch(const AutocompleteInput& input) {
  if (!IsAllowedInContext(input)) {
    return;
  }

  // Verify tiles age. Re-use previously cached response unless expired.
  if (!tiles_.empty() &&
      (base::TimeTicks::Now() - tiles_creation_timestamp_ <=
       base::Hours(OmniboxFieldTrial::kQueryTilesCacheMaxAge.Get()))) {
    return;
  }

  // Drop results, the contents will be served on Start().
  client_->GetQueryTileService()->GetQueryTiles(
      base::BindOnce(&QueryTileProvider::OnTilesFetched,
                     weak_ptr_factory_.GetWeakPtr(), /*is_prefetch=*/true));
}

void QueryTileProvider::Stop(bool clear_cached_results,
                             bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);

  // The request was stopped. Cancel any in-flight requests for fetching query
  // tiles from TileService.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool QueryTileProvider::IsAllowedInContext(const AutocompleteInput& input) {
  if (!client_->SearchSuggestEnabled())
    return false;

  if (client_->IsOffTheRecord())
    return false;

  if (!search::DefaultSearchProviderIsGoogle(
          client_->GetTemplateURLService())) {
    return false;
  }

  return (input.current_page_classification() ==
              metrics::OmniboxEventProto::NTP_ZPS_PREFETCH ||
          omnibox::IsNTPPage(input.current_page_classification())) &&
         input.focus_type() == metrics::INTERACTION_FOCUS;
}

void QueryTileProvider::OnTilesFetched(bool is_prefetch,
                                       std::vector<query_tiles::Tile> tiles) {
  tiles_ = std::move(tiles);
  tiles_creation_timestamp_ = base::TimeTicks::Now();
  if (!is_prefetch) {
    BuildSuggestions();
  }
}

void QueryTileProvider::BuildSuggestions() {
  // The following condition is guaranteed upon entering this function:
  // - if `Stop()` is called before callback is executed, the callback will be
  //   invalidated, and this method won't be invoked.
  // - `OnceCallback`s can only be executed once.
  // No other mechanism re-sets `done_`.
  DCHECK(!done_);
  done_ = true;

  // Note: we already know TemplateURL is valid, because we require DSE to be
  // Google -- see |IsAllowedInContext|.
  auto* template_url_service = client_->GetTemplateURLService();
  auto* template_url = template_url_service->GetDefaultSearchProvider();
  const auto& template_url_ref = template_url->url_ref();
  const auto& search_terms_data = template_url_service->search_terms_data();
  std::u16string keyword = template_url->keyword();

  for (const auto& tile : tiles_) {
    AutocompleteMatch match(this, kQueryTilesMatchRelevanceScore, false,
                            AutocompleteMatchType::TILE_SUGGESTION);
    match.contents = base::ASCIIToUTF16(tile.display_text);
    match.contents_class = ClassifyTermMatches({}, match.contents.size(),
                                               ACMatchClassification::MATCH,
                                               ACMatchClassification::URL);
    match.fill_into_edit = base::ASCIIToUTF16(tile.query_text);
    match.suggestion_group_id = omnibox::GROUP_MOBILE_QUERY_TILES;
    match.keyword = keyword;
    match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
        base::ASCIIToUTF16(tile.query_text));
    match.search_terms_args->additional_query_params =
        base::JoinString(tile.search_params, "&");
    if (!tile.image_metadatas.empty()) {
      match.image_url = tile.image_metadatas[0].url;
    }
    match.destination_url = GURL(template_url_ref.ReplaceSearchTerms(
        *match.search_terms_args, search_terms_data));

    matches_.push_back(std::move(match));
  }

  NotifyListeners(!matches_.empty());
}
