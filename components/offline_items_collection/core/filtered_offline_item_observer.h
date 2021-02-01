// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_FILTERED_OFFLINE_ITEM_OBSERVER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_FILTERED_OFFLINE_ITEM_OBSERVER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"

namespace offline_items_collection {

// Provides clients the ability to register observers interested only in the
// updates for a single offline item.
class FilteredOfflineItemObserver : public OfflineContentProvider::Observer {
 public:
  // Observer for a single offline item.
  class Observer {
   public:
    virtual void OnItemRemoved(const ContentId& id) = 0;
    virtual void OnItemUpdated(
        const OfflineItem& item,
        const base::Optional<UpdateDelta>& update_delta) = 0;

   protected:
    virtual ~Observer() = default;
  };

  FilteredOfflineItemObserver(OfflineContentProvider* provider);
  ~FilteredOfflineItemObserver() override;

  void AddObserver(const ContentId& id, Observer* observer);
  void RemoveObserver(const ContentId& id, Observer* observer);

 private:
  using ObserverValue = base::ObserverList<Observer>::Unchecked;
  using ObserversMap = std::map<ContentId, std::unique_ptr<ObserverValue>>;

  // OfflineContentProvider::Observer implementation.
  void OnItemsAdded(
      const OfflineContentProvider::OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item,
                     const base::Optional<UpdateDelta>& update_delta) override;

  OfflineContentProvider* provider_;
  ObserversMap observers_;

  DISALLOW_COPY_AND_ASSIGN(FilteredOfflineItemObserver);
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_FILTERED_OFFLINE_ITEM_OBSERVER_H_
