// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace offline_items_collection {

MockOfflineContentProvider::MockObserver::MockObserver() = default;
MockOfflineContentProvider::MockObserver::~MockObserver() = default;

MockOfflineContentProvider::MockOfflineContentProvider() {}
MockOfflineContentProvider::~MockOfflineContentProvider() = default;

bool MockOfflineContentProvider::HasObserver(Observer* observer) {
  return observers_.HasObserver(observer);
}

void MockOfflineContentProvider::SetItems(const OfflineItemList& items) {
  items_ = items;
}

void MockOfflineContentProvider::SetVisuals(
    std::map<ContentId, OfflineItemVisuals> visuals) {
  override_visuals_ = true;
  visuals_ = std::move(visuals);
}

void MockOfflineContentProvider::NotifyOnItemsAdded(
    const OfflineItemList& items) {
  for (auto& observer : observers_)
    observer.OnItemsAdded(items);
}

void MockOfflineContentProvider::NotifyOnItemRemoved(const ContentId& id) {
  for (auto& observer : observers_)
    observer.OnItemRemoved(id);
}

void MockOfflineContentProvider::NotifyOnItemUpdated(
    const OfflineItem& item,
    const base::Optional<UpdateDelta>& update_delta) {
  for (auto& observer : observers_)
    observer.OnItemUpdated(item, update_delta);
}

void MockOfflineContentProvider::GetVisualsForItem(const ContentId& id,
                                                   GetVisualsOptions options,
                                                   VisualsCallback callback) {
  if (!override_visuals_) {
    GetVisualsForItem_(id, options, std::move(callback));
  } else {
    std::unique_ptr<OfflineItemVisuals> visuals;
    auto iter = visuals_.find(id);
    if (iter != visuals_.end()) {
      visuals = std::make_unique<OfflineItemVisuals>(iter->second);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, std::move(visuals)));
  }
}

void MockOfflineContentProvider::GetAllItems(MultipleItemCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), items_));
}

void MockOfflineContentProvider::GetItemById(const ContentId& id,
                                             SingleItemCallback callback) {
  base::Optional<OfflineItem> result;
  for (auto item : items_) {
    if (item.id == id) {
      result = item;
      break;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void MockOfflineContentProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockOfflineContentProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace offline_items_collection
