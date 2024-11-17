// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/typed_count_sorter.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/titled_url_node.h"

namespace bookmarks {

using UrlTypedCountMap = BookmarkClient::UrlTypedCountMap;

namespace {

using UrlNodeMap =
    std::map<const GURL*, raw_ptr<const TitledUrlNode, CtnExperimental>>;
using UrlTypedCountPair = std::pair<const GURL*, int>;
using UrlTypedCountPairs = std::vector<UrlTypedCountPair>;

// Sort functor for UrlTypedCountPairs. We sort in decreasing order of typed
// count so that the best matches will always be added to the results.
struct UrlTypedCountPairSortFunctor {
  bool operator()(const UrlTypedCountPair& a,
                  const UrlTypedCountPair& b) const {
    return a.second > b.second;
  }
};

// Extract the GURL stored in an UrlTypedCountPair and use it to look up the
// corresponding TitledUrlNode.
class UrlTypedCountPairNodeLookupFunctor {
 public:
  explicit UrlTypedCountPairNodeLookupFunctor(UrlNodeMap& url_node_map)
      : url_node_map_(url_node_map) {}

  const TitledUrlNode* operator()(const UrlTypedCountPair& pair) const {
    return (*url_node_map_)[pair.first];
  }

 private:
  const raw_ref<UrlNodeMap> url_node_map_;
};

}  // namespace

TypedCountSorter::TypedCountSorter(BookmarkClient* client)
    : client_(client) {
  DCHECK(client_);
}

TypedCountSorter::~TypedCountSorter() = default;

void TypedCountSorter::SortMatches(const TitledUrlNodeSet& matches,
                                   TitledUrlNodes* sorted_nodes) const {
  sorted_nodes->reserve(matches.size());
  if (client_->SupportsTypedCountForUrls()) {
    UrlNodeMap url_node_map;
    UrlTypedCountMap url_typed_count_map;
    for (const TitledUrlNode* node : matches) {
      const GURL& url = node->GetTitledUrlNodeUrl();
      url_node_map.insert(std::make_pair(&url, node));
      url_typed_count_map.insert(std::make_pair(&url, 0));
    }

    client_->GetTypedCountForUrls(&url_typed_count_map);

    UrlTypedCountPairs url_typed_counts;
    base::ranges::copy(url_typed_count_map,
                       std::back_inserter(url_typed_counts));
    std::sort(url_typed_counts.begin(),
              url_typed_counts.end(),
              UrlTypedCountPairSortFunctor());
    base::ranges::transform(url_typed_counts, std::back_inserter(*sorted_nodes),
                            UrlTypedCountPairNodeLookupFunctor(url_node_map));
  } else {
    sorted_nodes->insert(sorted_nodes->end(), matches.begin(), matches.end());
  }
}

}  // namespace bookmarks
