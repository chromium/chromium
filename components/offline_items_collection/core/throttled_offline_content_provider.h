// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_THROTTLED_OFFLINE_CONTENT_PROVIDER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_THROTTLED_OFFLINE_CONTENT_PROVIDER_H_

#include <map>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace offline_items_collection {

// A simple wrapper around an OfflineContentProvider that throttles
// OfflineContentProvider::Observer::OnItemUpdated() calls to all registered
// observers. This class will coalesce updates to an item with an equal
// ContentId.
class ThrottledOfflineContentProvider
    : public OfflineContentProvider,
      public OfflineContentProvider::Observer {
 public:
  explicit ThrottledOfflineContentProvider(OfflineContentProvider* provider);
  ThrottledOfflineContentProvider(const base::TimeDelta& delay_between_updates,
                                  OfflineContentProvider* provider);

  ThrottledOfflineContentProvider(const ThrottledOfflineContentProvider&) =
      delete;
  ThrottledOfflineContentProvider& operator=(
      const ThrottledOfflineContentProvider&) = delete;

  ~ThrottledOfflineContentProvider() override;

  // Taking actions on the OfflineContentProvider will flush any queued updates
  // immediately after performing the action. This is to make sure item updates
  // in response to the update are immediately reflected back to the caller.
  void OpenItem(const OpenParams& open_params, const ContentId& id) override;
  void RemoveItem(const ContentId& id) override;
  void CancelDownload(const ContentId& id) override;
  void PauseDownload(const ContentId& id) override;
  void ResumeDownload(const ContentId& id) override;

  // Because this class queues updates, a call to Observer::OnItemUpdated might
  // get triggered with the same contents as returned by these getter methods in
  // the future.
  void GetItemById(const ContentId& id, SingleItemCallback callback) override;
  void GetAllItems(MultipleItemCallback callback) override;
  void GetVisualsForItem(const ContentId& id,
                         GetVisualsOptions options,
                         VisualsCallback callback) override;
  void GetShareInfoForItem(const ContentId& id,
                           ShareCallback callback) override;
  void RenameItem(const ContentId& id,
                  const std::string& name,
                  RenameCallback callback) override;

  // Visible for testing. Overrides the time at which this throttle last pushed
  // updates to observers.
  void set_last_update_time(const base::TimeTicks& t) { last_update_time_ = t; }

 private:
  // OfflineContentProvider::Observer implementation.
  void OnItemsAdded(const OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item,
                     const std::optional<UpdateDelta>& update_delta) override;
  void OnContentProviderGoingDown() override;

  void OnGetAllItemsDone(MultipleItemCallback callback,
                         const OfflineItemList& items);
  void OnGetItemByIdDone(SingleItemCallback callback,
                         const std::optional<OfflineItem>& item);

  // Checks if |item| already has an update pending. If so, replaces the content
  // of the update with |item|.
  void UpdateItemIfPresent(const OfflineItem& item);

  // Flushes all pending updates to the observers.
  void FlushUpdates();

  const base::TimeDelta delay_between_updates_;

  // Information about whether or not we're queuing updates.
  base::TimeTicks last_update_time_;
  bool update_queued_;

  const raw_ptr<OfflineContentProvider> wrapped_provider_;
  base::ScopedObservation<OfflineContentProvider,
                          OfflineContentProvider::Observer>
      observation_{this};

  typedef std::map<ContentId,
                   std::pair<OfflineItem, std::optional<UpdateDelta>>>
      OfflineItemMap;
  OfflineItemMap updates_;

  base::WeakPtrFactory<ThrottledOfflineContentProvider> weak_ptr_factory_{this};
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_THROTTLED_OFFLINE_CONTENT_PROVIDER_H_
