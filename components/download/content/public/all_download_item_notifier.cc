// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/public/all_download_item_notifier.h"

#include "base/trace_event/memory_usage_estimator.h"

namespace download {

AllDownloadItemNotifier::AllDownloadItemNotifier(
    content::DownloadManager* manager,
    AllDownloadItemNotifier::Observer* observer)
    : manager_(manager), observer_(observer) {
  DCHECK(observer_);
  manager_->AddObserver(this);
  content::DownloadManager::DownloadVector items;
  manager_->GetAllDownloads(&items);
  for (content::DownloadManager::DownloadVector::const_iterator it =
           items.begin();
       it != items.end(); ++it) {
    (*it)->AddObserver(this);
    observing_.insert(*it);
  }

  if (manager_->IsManagerInitialized())
    observer_->OnManagerInitialized(manager_);
}

AllDownloadItemNotifier::~AllDownloadItemNotifier() {
  if (manager_)
    manager_->RemoveObserver(this);
  for (auto it = observing_.begin(); it != observing_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  observing_.clear();
}

size_t AllDownloadItemNotifier::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(observing_);
}

void AllDownloadItemNotifier::OnManagerInitialized() {
  observer_->OnManagerInitialized(manager_);
}

void AllDownloadItemNotifier::ManagerGoingDown(
    content::DownloadManager* manager) {
  DCHECK_EQ(manager_, manager);

  // We might get deleted while calling `observer_->OnManagerGoingDown()`.
  // If so, the destructor will remove `this` as an observer of `manager_`.
  auto weak_ptr = weak_factory_.GetWeakPtr();
  observer_->OnManagerGoingDown(manager);
  if (!weak_ptr)
    return;

  manager_->RemoveObserver(this);
  manager_ = nullptr;
}

void AllDownloadItemNotifier::OnDownloadCreated(
    content::DownloadManager* manager,
    DownloadItem* item) {
  item->AddObserver(this);
  observing_.insert(item);
  observer_->OnDownloadCreated(manager, item);
}

void AllDownloadItemNotifier::OnDownloadUpdated(DownloadItem* item) {
  observer_->OnDownloadUpdated(manager_, item);
}

void AllDownloadItemNotifier::OnDownloadOpened(DownloadItem* item) {
  observer_->OnDownloadOpened(manager_, item);
}

void AllDownloadItemNotifier::OnDownloadRemoved(DownloadItem* item) {
  observer_->OnDownloadRemoved(manager_, item);
}

void AllDownloadItemNotifier::OnDownloadDestroyed(DownloadItem* item) {
  item->RemoveObserver(this);
  observing_.erase(item);
  observer_->OnDownloadDestroyed(manager_, item);
}

}  // namespace download
