// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/search/search_handler.h"

#include <algorithm>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/local_search_service/public/cpp/local_search_service_proxy.h"

namespace chromeos {
namespace help_app {

SearchHandler::SearchHandler(
    local_search_service::LocalSearchServiceProxy* local_search_service_proxy) {
  local_search_service_proxy->GetIndex(
      local_search_service::IndexId::kHelpAppLauncher,
      local_search_service::Backend::kInvertedIndex,
      index_remote_.BindNewPipeAndPassReceiver());
  DCHECK(index_remote_.is_bound());
}

SearchHandler::~SearchHandler() = default;

void SearchHandler::BindInterface(
    mojo::PendingReceiver<mojom::SearchHandler> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void SearchHandler::Search(const std::u16string& query,
                           uint32_t max_num_results,
                           SearchCallback callback) {
  // Search for 5x the maximum set of results. If there are many matches for
  // a query, it may be the case that |index_| returns some matches with higher
  // SearchResultDefaultRank values later in the list. Requesting up to 5x the
  // maximum number ensures that such results will be returned and can be ranked
  // accordingly when sorted.
  uint32_t max_local_search_service_results = 5 * max_num_results;

  index_remote_->Find(query, max_local_search_service_results,
                      base::BindOnce(&SearchHandler::OnFindComplete,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback), max_num_results));
}

std::vector<mojom::SearchResultPtr> SearchHandler::GenerateSearchResultsArray(
    const std::vector<local_search_service::Result>&
        local_search_service_results,
    uint32_t max_num_results) const {
  std::vector<mojom::SearchResultPtr> search_results;
  // TODO(b/182857903): Implement conversion from LSS result to search result.

  // TODO(b/182855408): Sort the search results.

  // Now that the results have been sorted, limit the size of to
  // |max_num_results|.
  // TODO(b/182857903): Test that limiting the number of results works.
  search_results.resize(
      std::min(static_cast<size_t>(max_num_results), search_results.size()));
  return search_results;
}

void SearchHandler::OnFindComplete(
    SearchCallback callback,
    uint32_t max_num_results,
    local_search_service::ResponseStatus response_status,
    const base::Optional<std::vector<local_search_service::Result>>&
        local_search_service_results) {
  if (response_status != local_search_service::ResponseStatus::kSuccess) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(GenerateSearchResultsArray(
      local_search_service_results.value(), max_num_results));
}

}  // namespace help_app
}  // namespace chromeos
