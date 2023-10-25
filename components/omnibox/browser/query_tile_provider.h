// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_QUERY_TILE_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_QUERY_TILE_PROVIDER_H_

#include <stddef.h>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"

namespace query_tiles {
struct Tile;
}  // namespace query_tiles

class AutocompleteProviderClient;
class AutocompleteProviderListener;

// An autocomplete provider that asynchronously provides matching
// query tiles to build the query tiles suggestion row.
// Only top level query tiles are displayed on New Tab Page in Zero Suggest.
class QueryTileProvider : public AutocompleteProvider {
 public:
  QueryTileProvider(AutocompleteProviderClient* client,
                    AutocompleteProviderListener* listener);

  QueryTileProvider(const QueryTileProvider& other) = delete;
  QueryTileProvider& operator=(const QueryTileProvider& other) = delete;

  // AutoCompleteProvider overrides.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void StartPrefetch(const AutocompleteInput& input) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;

  // Whether or not query tile suggestions are allowed in the given context.
  // Invoked early, confirms all the external conditions for query tiles are
  // met.
  bool IsAllowedInContext(const AutocompleteInput& input);

 private:
  FRIEND_TEST_ALL_PREFIXES(QueryTileProviderTest, StartPrefetch_Start);
  FRIEND_TEST_ALL_PREFIXES(QueryTileProviderTest, Start_Stop_Start);
  FRIEND_TEST_ALL_PREFIXES(QueryTileProviderTest,
                           StartPrefetch_CacheExpirationTest);
  ~QueryTileProvider() override;

  // Callback invoked in response to (pre-)fetching the top level tiles from
  // TileService.
  void OnTilesFetched(bool is_prefetch, std::vector<query_tiles::Tile> tiles);

  // Builds one query tile suggestion for each tile reported in |tiles_|.
  void BuildSuggestions();

  const raw_ptr<AutocompleteProviderClient> client_;
  std::vector<query_tiles::Tile> tiles_;
  base::TimeTicks tiles_creation_timestamp_;
  base::WeakPtrFactory<QueryTileProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_QUERY_TILE_PROVIDER_H_
