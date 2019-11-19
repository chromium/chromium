// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_THROTTLED_OFFLINE_CONTENT_PROVIDER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_THROTTLED_OFFLINE_CONTENT_PROVIDER_H_

#include <map>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
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
  ~ThrottledOfflineContentProvider() override;

  // Taking actions on the OfflineContentProvider will flush any queued updates
  // immediately after performing the action. This is to make sure item updates
  // in response to the update are immediately reflected back to the caller.
  void OpenItem(LaunchLocation location, const ContentId& id) override;
  void RemoveItem(const ContentId& id) override;
  void CancelDownload(const ContentId& id) override;
  void PauseDownload(const ContentId& id) override;
  void ResumeDownload(const ContentId& id, bool has_user_gesture) override;

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
  void AddObserver(OfflineContentProvider::Observer* observer) override;
  void RemoveObserver(OfflineContentProvider::Observer* observer) override;

  // Visible for testing. Overrides the time at which this throttle last pushed
  // updates to observers.
  void set_last_update_time(const base::TimeTicks& t) { last_update_time_ = t; }

 private:
  // OfflineContentProvider::Observer implementation.
  void OnItemsAdded(const OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item,
                     const base::Optional<UpdateDelta>& update_delta) override;

  void OnGetAllItemsDone(MultipleItemCallback callback,
                         const OfflineItemList& items);
  void OnGetItemByIdDone(SingleItemCallback callback,
                         const base::Optional<OfflineItem>& item);

  // Checks if |item| already has an update pending. If so, replaces the content
  // of the update with |item|.
  void UpdateItemIfPresent(const OfflineItem& item);

  // Flushes all pending updates to the observers.
  void FlushUpdates();

  const base::TimeDelta delay_between_updates_;

  // Information about whether or not we're queuing updates.
  base::TimeTicks last_update_time_;
  bool update_queued_;

  OfflineContentProvider* const wrapped_provider_;
  base::ObserverList<OfflineContentProvider::Observer>::Unchecked observers_;

  typedef std::map<ContentId,
                   std::pair<OfflineItem, base::Optional<UpdateDelta>>>
      OfflineItemMap;
  OfflineItemMap updates_;

  base::WeakPtrFactory<ThrottledOfflineContentProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ThrottledOfflineContentProvider);
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLETION_CORE_THROTTLED_OFFLINE_CONTENT_PROVIDER_H_
