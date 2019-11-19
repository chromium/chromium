// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_FAVICON_CACHE_H_
#define COMPONENTS_SYNC_SESSIONS_FAVICON_CACHE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/core/session_id.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "url/gurl.h"

namespace chrome {
struct FaviconRawBitmapResult;
}

namespace favicon {
class FaviconService;
}

namespace sync_sessions {

enum IconSize {
  SIZE_INVALID,
  SIZE_16,
  SIZE_32,
  SIZE_64,
  NUM_SIZES
};

struct SyncedFaviconInfo;

// FAVICON SYNC IS DEPRECATED: This class now only serves to the translation
// from page url to icon url using sessions sync information.
// TODO(https://crbug.com/978775): Stop implementing syncer::SyncableService
// and rename the class accordingly.
// TODO(zea): make this a KeyedService.
// Encapsulates the logic for loading and storing synced favicons.
class FaviconCache : public syncer::SyncableService,
                     public history::HistoryServiceObserver {
 public:
  FaviconCache(favicon::FaviconService* favicon_service,
               history::HistoryService* history_service,
               int max_sync_favicon_limit);
  ~FaviconCache() override;

  // SyncableService implementation.
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // If a valid favicon for the icon at |favicon_url| is found, returns a
  // pointer to the png-encoded image. Otherwise, returns nullptr.
  favicon_base::FaviconRawBitmapResult GetSyncedFaviconForFaviconURL(
      const GURL& favicon_url) const;

  // Returns the value associated with |page_url| in |page_favicon_map_| if one
  // exists, otherwise returns an empty URL.
  GURL GetIconUrlForPageUrl(const GURL& page_url) const;

  // If a valid favicon for the icon associated with |page_url| is found,
  // returns a pointer to the png-encoded image. Otherwise, returns nullptr.
  favicon_base::FaviconRawBitmapResult GetSyncedFaviconForPageURL(
      const GURL& page_url) const;

  // Load the favicon for |page_url|. Will create a new sync node or update
  // an existing one as necessary, and update the last visit time with |mtime|,
  // Only those favicon types defined in SupportedFaviconTypes will be synced.
  void OnPageFaviconUpdated(const GURL& page_url, base::Time mtime);

  // Update the visit count for the favicon associated with |favicon_url|.
  // If no favicon exists associated with |favicon_url|, triggers a load
  // for the favicon associated with |page_url|.
  void OnFaviconVisited(const GURL& page_url, const GURL& favicon_url);

  // Consume Session sync favicon data to update the in-memory page->favicon url
  // mappings and visit times.
  void UpdateMappingsFromForeignTab(const sync_pb::SessionTab& tab,
                                    base::Time visit_time);

  base::WeakPtr<FaviconCache> GetWeakPtr();

  // For testing only.
  size_t NumFaviconsForTest() const;
  size_t NumTasksForTest() const;
  base::Time GetLastVisitTimeForTest(const GURL& favicon_url) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SyncFaviconCacheTest, HistoryFullClear);
  FRIEND_TEST_ALL_PREFIXES(SyncFaviconCacheTest, HistorySubsetClear);

  // Functor for ordering SyncedFaviconInfo objects by recency;
  struct FaviconRecencyFunctor {
    bool operator()(const SyncedFaviconInfo* lhs,
                    const SyncedFaviconInfo* rhs) const;
  };


  // Map of favicon url to favicon image.
  using FaviconMap = std::map<GURL, std::unique_ptr<SyncedFaviconInfo>>;
  using RecencySet = std::set<SyncedFaviconInfo*, FaviconRecencyFunctor>;

  // Callback method to store a tab's favicon into its sync node once it becomes
  // available. Does nothing if no favicon data was available.
  void OnFaviconDataAvailable(
      const GURL& page_url,
      base::Time mtime,
      const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_result);

  // Helper method to update the sync state of the favicon at |icon_url|. If
  // either |image_change_type| or |tracking_change_type| is ACTION_INVALID,
  // the corresponding datatype won't be updated.
  // Note: should only be called after both FAVICON_IMAGES and FAVICON_TRACKING
  // have been successfully set up.
  void UpdateSyncState(const GURL& icon_url,
                       syncer::SyncChange::SyncChangeType image_change_type,
                       syncer::SyncChange::SyncChangeType tracking_change_type);

  // Helper method to get favicon info from |synced_favicons_|. If no info
  // exists for |icon_url|, creates a new SyncedFaviconInfo in both
  // |synced_favicons_| and |recent_favicons_| and returns it.
  SyncedFaviconInfo* GetFaviconInfo(const GURL& icon_url);

  // Updates the last visit time for the favicon at |icon_url| to |time| and
  // correspondingly updates position in |recent_favicons_|.
  void UpdateFaviconVisitTime(const GURL& icon_url, base::Time time);

  // Expiration method. Looks through |recent_favicons_| to find any favicons
  // that should be expired in order to maintain the sync favicon limit,
  // appending deletions to |image_changes| and |tracking_changes| as necessary.
  void ExpireFaviconsIfNecessary(syncer::SyncChangeList* image_changes,
                                 syncer::SyncChangeList* tracking_changes);

  // Returns the local favicon url associated with |sync_favicon| if one exists
  // in |synced_favicons_|, else returns an invalid GURL.
  GURL GetLocalFaviconFromSyncedData(
      const syncer::SyncData& sync_favicon) const;

  // Merges |sync_favicon| into |synced_favicons_|, updating |local_changes|
  // with any changes that should be pushed to the sync processor.
  void MergeSyncFavicon(const syncer::SyncData& sync_favicon,
                        syncer::SyncChangeList* sync_changes);

  // Updates |synced_favicons_| with the favicon data from |sync_favicon|.
  void AddLocalFaviconFromSyncedData(const syncer::SyncData& sync_favicon);

  // Creates a SyncData object from the |type| data of |favicon_url|
  // from within |synced_favicons_|.
  syncer::SyncData CreateSyncDataFromLocalFavicon(
      syncer::ModelType type,
      const GURL& favicon_url) const;

  // Deletes all synced favicons corresponding with |favicon_urls| and pushes
  // the deletions to sync.
  void DeleteSyncedFavicons(const std::set<GURL>& favicon_urls);

  // Deletes the favicon pointed to by |favicon_iter| and appends the necessary
  // sync deletions to |image_changes| and |tracking_changes|.
  void DeleteSyncedFavicon(FaviconMap::iterator favicon_iter,
                           syncer::SyncChangeList* image_changes,
                           syncer::SyncChangeList* tracking_changes);

  // Locally drops the favicon pointed to by |favicon_iter|.
  void DropSyncedFavicon(FaviconMap::iterator favicon_iter);

  // Only drops the data associated with |type| of |favicon_iter|.
  void DropPartialFavicon(FaviconMap::iterator favicon_iter,
                          syncer::ModelType type);

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  void OnHistoryServiceLoaded(
      history::HistoryService* history_service) override;

  favicon::FaviconService* const favicon_service_;
  history::HistoryService* const history_service_;

  // Task tracker for loading favicons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Our actual cached favicon data. Owns the favicons.
  FaviconMap synced_favicons_;

  // An LRU ordering of the favicons comprising |synced_favicons_| (oldest to
  // newest).
  RecencySet recent_favicons_;

  // Pending favicon loads, map of page url to task id.
  std::map<GURL, base::CancelableTaskTracker::TaskId> page_task_map_;

  // Map of page url to favicon url.
  std::map<GURL, GURL> page_favicon_map_;

  // TODO(zea): consider creating a favicon handler here for fetching unsynced
  // favicons from the web.

  std::unique_ptr<syncer::SyncChangeProcessor> favicon_images_sync_processor_;
  std::unique_ptr<syncer::SyncChangeProcessor> favicon_tracking_sync_processor_;

  // Maximum number of favicons to sync. 0 means no limit.
  const size_t max_sync_favicon_limit_;

  // A vector is needed to support concurrent calls to WaitUntilReadyToSync()
  // because this class powers two datatypes.
  std::vector<base::OnceClosure> wait_until_ready_to_sync_cb_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_{this};

  // Weak pointer factory for favicon loads.
  base::WeakPtrFactory<FaviconCache> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FaviconCache);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_FAVICON_CACHE_H_
