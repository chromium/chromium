// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/offline_content_provider.h"
#include "base/observer_list.h"

namespace offline_items_collection {

OfflineContentProvider::OfflineContentProvider() = default;

OfflineContentProvider::~OfflineContentProvider() {
  for (auto& observer : observers_)
    observer.OnContentProviderGoingDown();
}

void OfflineContentProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OfflineContentProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool OfflineContentProvider::HasObserver(Observer* observer) {
  return observers_.HasObserver(observer);
}

void OfflineContentProvider::NotifyItemsAdded(const OfflineItemList& items) {
  for (auto& observer : observers_)
    observer.OnItemsAdded(items);
}

void OfflineContentProvider::NotifyItemRemoved(const ContentId& id) {
  for (auto& observer : observers_)
    observer.OnItemRemoved(id);
}

void OfflineContentProvider::NotifyItemUpdated(
    const OfflineItem& item,
    const std::optional<UpdateDelta>& update_delta) {
  for (auto& observer : observers_)
    observer.OnItemUpdated(item, update_delta);
}

}  // namespace offline_items_collection
