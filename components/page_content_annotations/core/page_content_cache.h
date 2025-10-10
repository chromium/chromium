// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

class GURL;

namespace os_crypt_async {
class Encryptor;
class OSCryptAsync;
}  // namespace os_crypt_async

namespace optimization_guide {
class PageContentStore;
}  // namespace optimization_guide

namespace page_content_annotations {

// Caches page content annotations and provides methods to interact with the
// underlying store. All database operations are done on a background thread.
class PageContentCache {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the content for `tab_id` has been added to the cache.
    virtual void OnCachePopulated(int64_t tab_id) {}
    // Called when the content for `tab_id` has been removed from the cache.
    virtual void OnCacheRemoved(int64_t tab_id) {}
  };

 public:
  PageContentCache(os_crypt_async::OSCryptAsync* os_crypt_async,
                   const base::FilePath& profile_dir);
  ~PageContentCache();

  PageContentCache(const PageContentCache&) = delete;
  PageContentCache& operator=(const PageContentCache&) = delete;

  using GetPageContentCallback = base::OnceCallback<void(
      std::optional<optimization_guide::proto::PageContext>)>;
  using GetAllTabIdsCallback = base::OnceCallback<void(std::vector<int64_t>)>;

  // Retrieves the page content for a given tab ID.
  void GetPageContentForTab(int64_t tab_id, GetPageContentCallback callback);

  // Retrieves all tab IDs from the cache that has page contents cached.
  void GetAllTabIds(GetAllTabIdsCallback callback);

  // Calculates and records cache-related metrics.
  void RecordMetrics(std::set<int64_t> eligible_tab_ids);

  // Called when a tab is backgrounded. See PageContentStore::AddPageContent().
  void CachePageContent(
      int64_t tab_id,
      const GURL& url,
      const base::Time& visit_timestamp,
      const base::Time& extraction_timestamp,
      const optimization_guide::proto::PageContext& page_context);

  // Called when a tab is updated or closed. This will remove any contents
  // stored for the tab.
  void RemovePageContentForTab(int64_t tab_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void OnOsCryptAsyncReady(os_crypt_async::Encryptor encryptor);
  void OnCacheSizeCalculated(std::set<int64_t> eligible_tab_ids,
                             int64_t total_cache_size);
  void OnReceiveAllCachedTabIds(int64_t total_cache_size,
                                std::set<int64_t> eligible_tab_ids,
                                std::vector<int64_t> cached_tab_ids);
  void OnStoreInitialized();

  // Deletes old data from the store.
  void DeleteOldData();

  const base::FilePath database_path_;

  // `true` once `store_` has been initialized.
  bool store_initialized_ = false;

  // Tasks that should be run once `store_` has been initialized.
  std::vector<base::OnceClosure> pending_tasks_;

  base::SequenceBound<optimization_guide::PageContentStore> store_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<PageContentCache> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_H_
