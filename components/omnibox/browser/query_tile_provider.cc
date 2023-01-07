// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/query_tile_provider.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/query_tiles/tile_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_fixer.h"

namespace {

// The relevance score for query tile match.
const int kQueryTilesMatchRelevanceScore = 1599;

// Helper function to determine if the omnibox text matches the previously
// selected |tile_query_text|. If |tile_query_text| is empty, empty input
// text is considered a match.
bool TextMatchesTileQueryText(const std::u16string& input_text,
                              const std::string& tile_query_text) {
  auto trimmed_input =
      base::TrimWhitespace(input_text, base::TrimPositions::TRIM_TRAILING);
  auto tile_text = base::UTF8ToUTF16(tile_query_text);
  auto trimmed_tile_text =
      base::TrimWhitespace(tile_text, base::TrimPositions::TRIM_TRAILING);
  return trimmed_input == trimmed_tile_text;
}

// Helper function to determine if we are currently showing a URL and the
// omnibox text matches this URL.
bool TextMatchesPageURL(const AutocompleteInput& input) {
  const GURL fixed_url(url_formatter::FixupURL(base::UTF16ToUTF8(input.text()),
                                               /*desired_tld=*/std::string()));
  return input.current_url() == fixed_url;
}

const TemplateURL* GetDefaultSearchProvider(
    AutocompleteProviderClient* client) {
  TemplateURLService* template_url_service = client->GetTemplateURLService();
  return template_url_service ? template_url_service->GetDefaultSearchProvider()
                              : nullptr;
}

}  // namespace

QueryTileProvider::QueryTileProvider(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_QUERY_TILE),
      client_(client),
      tile_service_(client_->GetQueryTileService()) {
  AddListener(listener);
}

QueryTileProvider::~QueryTileProvider() = default;

void QueryTileProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  done_ = input.omit_asynchronous_matches();
  matches_.clear();
  if (!AllowQueryTileSuggestions(input)) {
    done_ = true;
    return;
  }

  // If the request was started by tapping on a tile, fetch the sub-tiles.
  // Otherwise, try showing the top level tiles.
  if (input.query_tile_id().has_value()) {
    tile_service_->GetTile(
        input.query_tile_id().value(),
        base::BindOnce(&QueryTileProvider::OnSubTilesFetched,
                       weak_ptr_factory_.GetWeakPtr(), input));
  } else {
    tile_service_->GetQueryTiles(
        base::BindOnce(&QueryTileProvider::OnTopLevelTilesFetched,
                       weak_ptr_factory_.GetWeakPtr(), input));
  }
}

void QueryTileProvider::Stop(bool clear_cached_results,
                             bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);

  // The request was stopped. Cancel any in-flight requests for fetching query
  // tiles from TileService.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool QueryTileProvider::AllowQueryTileSuggestions(
    const AutocompleteInput& input) {
  if (!client_->SearchSuggestEnabled())
    return false;

  if (client_->IsOffTheRecord())
    return false;

  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  const TemplateURL* default_provider = GetDefaultSearchProvider(client_);
  bool is_search_provider_enabled =
      default_provider &&
      default_provider->GetEngineType(
          template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;
  if (!is_search_provider_enabled)
    return false;

  // Only show suggestions for NTP or schemes that users recognize.
  const auto& page_url = input.current_url();
  bool is_supported_scheme =
      page_url.is_valid() &&
      ((page_url.scheme() == url::kHttpScheme) ||
       (page_url.scheme() == url::kHttpsScheme) ||
       (page_url.scheme() == url::kAboutScheme) ||
       (page_url.scheme() ==
        client_->GetEmbedderRepresentationOfAboutScheme()));
  if (input.type() == metrics::OmniboxInputType::URL && !is_supported_scheme)
    return false;

  return true;
}

void QueryTileProvider::OnTopLevelTilesFetched(
    const AutocompleteInput& input,
    std::vector<query_tiles::Tile> tiles) {
  BuildSuggestion(input, /*tile_query_text=*/"", std::move(tiles));
}

void QueryTileProvider::OnSubTilesFetched(
    const AutocompleteInput& input,
    absl::optional<query_tiles::Tile> tile) {
  DCHECK(tile.has_value());
  std::vector<query_tiles::Tile> sub_tiles;
  for (const auto& sub_tile : std::move(tile->sub_tiles))
    sub_tiles.emplace_back(std::move(*sub_tile.get()));

  BuildSuggestion(input, tile->query_text, std::move(sub_tiles));
}

void QueryTileProvider::BuildSuggestion(const AutocompleteInput& input,
                                        const std::string& tile_query_text,
                                        std::vector<query_tiles::Tile> tiles) {
  if (done_)
    return;

  done_ = true;

  if (tiles.empty())
    return;

  bool is_showing_tile_text =
      TextMatchesTileQueryText(input.text(), tile_query_text);
  bool is_showing_url = input.type() == metrics::OmniboxInputType::URL &&
                        TextMatchesPageURL(input);
  bool show_query_tiles = is_showing_tile_text || is_showing_url;
  if (!show_query_tiles)
    return;

  AutocompleteMatch match(this, kQueryTilesMatchRelevanceScore, true,
                          AutocompleteMatchType::TILE_SUGGESTION);
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(input.text());
  const TemplateURL* default_provider = GetDefaultSearchProvider(client_);
  match.keyword =
      default_provider ? default_provider->keyword() : std::u16string();
  match.query_tiles = std::move(tiles);

  // The query tiles suggestion is shown as the default suggestion, unless there
  // is an edit URL suggestion which is shown as the first suggestion.
  if (!is_showing_url)
    match.SetAllowedToBeDefault(input);

  matches_.push_back(match);
  NotifyListeners(true);
}
