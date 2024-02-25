// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace offline_items_collection {

MockOfflineContentProvider::MockObserver::MockObserver() = default;
MockOfflineContentProvider::MockObserver::~MockObserver() = default;

MockOfflineContentProvider::MockOfflineContentProvider() {}
MockOfflineContentProvider::~MockOfflineContentProvider() = default;

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
  NotifyItemsAdded(items);
}

void MockOfflineContentProvider::NotifyOnItemRemoved(const ContentId& id) {
  NotifyItemRemoved(id);
}

void MockOfflineContentProvider::NotifyOnItemUpdated(
    const OfflineItem& item,
    const std::optional<UpdateDelta>& update_delta) {
  NotifyItemUpdated(item, update_delta);
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, std::move(visuals)));
  }
}

void MockOfflineContentProvider::GetAllItems(MultipleItemCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), items_));
}

void MockOfflineContentProvider::GetItemById(const ContentId& id,
                                             SingleItemCallback callback) {
  std::optional<OfflineItem> result;
  for (auto item : items_) {
    if (item.id == id) {
      result = item;
      break;
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace offline_items_collection
