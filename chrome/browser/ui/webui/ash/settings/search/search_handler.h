// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_SEARCH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_SEARCH_HANDLER_H_

#include <optional>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/search.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::settings {

class Hierarchy;
class OsSettingsSections;
struct SearchConcept;

// Handles search queries for Chrome OS settings. Search() is expected to be
// invoked by the settings UI as well as the the Launcher search UI. Search
// results are obtained by matching the provided query against search tags
// indexed in the LocalSearchService and cross-referencing results with
// SearchTagRegistry.
//
// Searches which do not provide any matches result in an empty results array.
class SearchHandler : public mojom::SearchHandler,
                      public SearchTagRegistry::Observer {
 public:
  SearchHandler(SearchTagRegistry* search_tag_registry,
                OsSettingsSections* sections,
                Hierarchy* hierarchy,
                local_search_service::LocalSearchServiceProxy*
                    local_search_service_proxy);
  ~SearchHandler() override;

  SearchHandler(const SearchHandler& other) = delete;
  SearchHandler& operator=(const SearchHandler& other) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::SearchHandler> pending_receiver);

  // mojom::SearchHandler:
  void Search(const std::u16string& query,
              uint32_t max_num_results,
              mojom::ParentResultBehavior parent_result_behavior,
              SearchCallback callback) override;
  void Observe(
      mojo::PendingRemote<mojom::SearchResultsObserver> observer) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchHandlerTest, CompareSearchResults);

  // SearchTagRegistry::Observer:
  void OnRegistryUpdated() override;

  std::vector<mojom::SearchResultPtr> GenerateSearchResultsArray(
      const std::vector<local_search_service::Result>&
          local_search_service_results,
      uint32_t max_num_results,
      mojom::ParentResultBehavior parent_result_behavior) const;

  void OnFindComplete(
      SearchCallback callback,
      uint32_t max_num_results,
      mojom::ParentResultBehavior parent_result_behavior,
      local_search_service::ResponseStatus response_status,
      const std::optional<std::vector<local_search_service::Result>>&
          local_search_service_results);

  void AddParentResults(
      uint32_t max_num_results,
      std::vector<mojom::SearchResultPtr>* search_results) const;

  std::vector<mojom::SearchResultPtr>::iterator AddSectionResultIfPossible(
      const std::vector<mojom::SearchResultPtr>::iterator& position,
      const mojom::SearchResultPtr& child_result,
      chromeos::settings::mojom::Section section,
      std::vector<mojom::SearchResultPtr>* results) const;

  std::vector<mojom::SearchResultPtr>::iterator AddSubpageResultIfPossible(
      const std::vector<mojom::SearchResultPtr>::iterator& position,
      const mojom::SearchResultPtr& child_result,
      chromeos::settings::mojom::Subpage subpage,
      double relevance_score,
      std::vector<mojom::SearchResultPtr>* results) const;

  mojom::SearchResultPtr ResultToSearchResult(
      const local_search_service::Result& result) const;
  std::string GetModifiedUrl(const SearchConcept& search_concept,
                             chromeos::settings::mojom::Section section) const;

  // Returns true if |first| should be ranked before |second|.
  static bool CompareSearchResults(const mojom::SearchResultPtr& first,
                                   const mojom::SearchResultPtr& second);

  raw_ptr<SearchTagRegistry> search_tag_registry_;
  raw_ptr<OsSettingsSections> sections_;
  raw_ptr<Hierarchy> hierarchy_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;

  // Note: Expected to have multiple clients, so ReceiverSet/RemoteSet are used.
  mojo::ReceiverSet<mojom::SearchHandler> receivers_;
  mojo::RemoteSet<mojom::SearchResultsObserver> observers_;

  base::WeakPtrFactory<SearchHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_SEARCH_HANDLER_H_
