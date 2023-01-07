// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_QUERY_TILE_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_QUERY_TILE_PROVIDER_H_

#include <stddef.h>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace query_tiles {
class TileService;
struct Tile;
}  // namespace query_tiles

class AutocompleteProviderClient;
class AutocompleteProviderListener;

// An autocomplete provider that asynchronously provides matching
// query tiles to build the query tiles suggestion row according to the
// following.
// (1) For zero suggest, top level query tiles will be displayed.
// (2) If user taps on a query tile, the suggestion will be replaced by the next
//     level query tiles for the selected tile.
// (3) If user starts typing, the query tile suggestion will be dismissed.
class QueryTileProvider : public AutocompleteProvider {
 public:
  QueryTileProvider(AutocompleteProviderClient* client,
                    AutocompleteProviderListener* listener);

  QueryTileProvider(const QueryTileProvider& other) = delete;
  QueryTileProvider& operator=(const QueryTileProvider& other) = delete;

  // AutoCompleteProvider overrides.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;

 private:
  ~QueryTileProvider() override;

  // Whether or not query tile suggestions are allowed in the given context.
  // Invoked early, confirms all the external conditions for query tiles are
  // met.
  bool AllowQueryTileSuggestions(const AutocompleteInput& input);

  // Callback invoked in response to fetching the top level tiles from
  // TileService.
  void OnTopLevelTilesFetched(const AutocompleteInput& input,
                              std::vector<query_tiles::Tile> tiles);

  // Callback invoked in response to fetching subtiles of a selected query tile
  // from TileService.
  void OnSubTilesFetched(const AutocompleteInput& input,
                         absl::optional<query_tiles::Tile> tile);

  // For the given |input| and optionally a selected tile denoted by
  // |tile_query_text|, checks if a suggestion should be shown. If yes, builds a
  // query tile suggestion with the matching |tiles|.
  void BuildSuggestion(const AutocompleteInput& input,
                       const std::string& tile_query_text,
                       std::vector<query_tiles::Tile> tiles);

  const raw_ptr<AutocompleteProviderClient> client_;

  // The backend providing query tiles.
  const raw_ptr<query_tiles::TileService> tile_service_;

  base::WeakPtrFactory<QueryTileProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_QUERY_TILE_PROVIDER_H_
