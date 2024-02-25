// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_row_list_view_info.h"

DownloadBubbleRowListViewInfoObserver::DownloadBubbleRowListViewInfoObserver() =
    default;

DownloadBubbleRowListViewInfoObserver::
    ~DownloadBubbleRowListViewInfoObserver() {
  CHECK(!IsInObserverList());
}

DownloadBubbleRowListViewInfo::DownloadBubbleRowListViewInfo(
    std::vector<DownloadUIModel::DownloadUIModelPtr> models) {
  CHECK(!models.empty());

  if (!models.front()->GetEndTime().is_null()) {
    last_completed_time_ = models.front()->GetEndTime();
  }

  for (DownloadUIModel::DownloadUIModelPtr& model : models) {
    AddRow(std::move(model));
  }
}

DownloadBubbleRowListViewInfo::~DownloadBubbleRowListViewInfo() = default;

void DownloadBubbleRowListViewInfo::OnDownloadDestroyed(
    const offline_items_collection::ContentId& id) {
  RemoveRow(id);
}

const DownloadBubbleRowViewInfo* DownloadBubbleRowListViewInfo::GetRowInfo(
    const offline_items_collection::ContentId& id) const {
  auto it = row_list_iter_map_.find(id);
  if (it == row_list_iter_map_.end()) {
    return nullptr;
  }
  return &(*it->second);
}

void DownloadBubbleRowListViewInfo::AddRow(
    DownloadUIModel::DownloadUIModelPtr model) {
  offline_items_collection::ContentId id = model->GetContentId();
  rows_.emplace_back(std::move(model)).AddObserver(this);
  auto it = std::prev(rows_.end());
  row_list_iter_map_.emplace(id, it);
  NotifyObservers(&DownloadBubbleRowListViewInfoObserver::OnRowAdded, id);
}

void DownloadBubbleRowListViewInfo::RemoveRow(
    const offline_items_collection::ContentId& id) {
  auto it = row_list_iter_map_.find(id);
  if (it == row_list_iter_map_.end()) {
    return;
  }

  NotifyObservers(&DownloadBubbleRowListViewInfoObserver::OnRowWillBeRemoved,
                  id);
  rows_.erase(it->second);
  row_list_iter_map_.erase(it);
  NotifyObservers(&DownloadBubbleRowListViewInfoObserver::OnAnyRowRemoved);
}
