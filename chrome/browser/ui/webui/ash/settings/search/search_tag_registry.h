// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_SEARCH_TAG_REGISTRY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_SEARCH_TAG_REGISTRY_H_

#include <unordered_map>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::settings {

struct SearchConcept;

// Processes all registered search tags by adding/removing them from
// LocalSearchService and providing metadata via GetTagMetadata().
class SearchTagRegistry {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnRegistryUpdated() = 0;
  };

  class ScopedTagUpdater {
   public:
    ScopedTagUpdater(ScopedTagUpdater&&);
    ScopedTagUpdater(const ScopedTagUpdater&) = delete;
    ScopedTagUpdater& operator=(const ScopedTagUpdater&) = delete;
    ~ScopedTagUpdater();

    void AddSearchTags(const std::vector<SearchConcept>& search_tags);
    void RemoveSearchTags(const std::vector<SearchConcept>& search_tags);

   private:
    friend class SearchTagRegistry;

    explicit ScopedTagUpdater(SearchTagRegistry* registry);

    void ProcessPendingSearchTags(const std::vector<SearchConcept>& search_tags,
                                  bool is_pending_add);

    raw_ptr<SearchTagRegistry> registry_;

    // A SearchConcept along with a bool of the pending update state. If the
    // bool is true, the concept should be added; if the bool is false, the
    // concept should be removed.
    using ConceptWithShouldAddBool = std::pair<const SearchConcept*, bool>;
    std::unordered_map<std::string, ConceptWithShouldAddBool> pending_updates_;
  };

  explicit SearchTagRegistry(local_search_service::LocalSearchServiceProxy*
                                 local_search_service_proxy);
  SearchTagRegistry(const SearchTagRegistry& other) = delete;
  SearchTagRegistry& operator=(const SearchTagRegistry& other) = delete;
  virtual ~SearchTagRegistry();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts a tag update, which allows clients to add/remove search tags. The
  // ScopedTagUpdater object is returned by value and only updates tags when it
  // goes out of scope, so clients should not hold onto it outside the scope of
  // a function.
  ScopedTagUpdater StartUpdate();

  // Returns the tag metadata associated with |result_id|, which is the ID
  // returned by the LocalSearchService. If no metadata is available, null
  // is returned.
  const SearchConcept* GetTagMetadata(const std::string& result_id) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchTagRegistryTest, AddAndRemove);

  static std::string ToResultId(const SearchConcept& search_concept);

  void AddSearchTags(const std::vector<const SearchConcept*>& search_tags);
  void RemoveSearchTags(const std::vector<const SearchConcept*>& search_tags);

  std::vector<local_search_service::Data> ConceptVectorToDataVector(
      const std::vector<const SearchConcept*>& search_tags);
  void NotifyRegistryUpdated();
  void NotifyRegistryAdded();
  void NotifyRegistryDeleted(uint32_t /*num_deleted*/);

  // Index used by the LocalSearchService for string matching.
  mojo::Remote<local_search_service::mojom::Index> index_remote_;

  // In-memory cache of all results which have been added to the
  // LocalSearchService. Contents are kept in sync with |index_remote_|.
  std::unordered_map<std::string, raw_ptr<const SearchConcept, CtnExperimental>>
      result_id_to_metadata_list_map_;

  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<SearchTagRegistry> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_SEARCH_TAG_REGISTRY_H_
