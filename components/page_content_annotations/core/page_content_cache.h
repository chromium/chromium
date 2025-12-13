// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_H_

#include <set>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

class GURL;

namespace base {
class TimeDelta;
}  // namespace base

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
                   const base::FilePath& profile_dir,
                   base::TimeDelta max_context_age);
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

  // Called when the tab state is initialized to perform cleanup of stale
  // entries.
  void RunCleanUpTasksWithActiveTabs(const std::set<int64_t>& all_tab_ids);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void PostDelayedCleanUpTask(const std::set<int64_t>& all_active_tab_ids,
                              std::vector<int64_t> cached_tab_ids);
  void CleanUpAndRecordMetrics(const std::set<int64_t>& all_active_tab_ids,
                               const std::set<int64_t>& stale_tab_ids,
                               const std::set<int64_t>& cached_tab_ids);
  void OnOsCryptAsyncReady(os_crypt_async::Encryptor encryptor);
  void OnCacheSizeCalculated(const std::set<int64_t>& all_active_tab_ids,
                             const std::set<int64_t>& cached_tab_ids,
                             std::optional<int64_t> total_cache_size_optional);
  void OnStoreInitialized();

  // Deletes old data from the store.
  void DeleteOldData();

  const base::FilePath database_path_;

  // `true` once `store_` has been initialized.
  bool store_initialized_ = false;

  // The maximum age of page contexts in the cache not deleted at cleanup time.
  base::TimeDelta max_context_age_;

  // Tasks that should be run once `store_` has been initialized.
  std::vector<base::OnceClosure> pending_tasks_;

  base::SequenceBound<optimization_guide::PageContentStore> store_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<PageContentCache> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_H_
