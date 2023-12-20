// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/all_download_event_notifier.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

namespace download {

AllDownloadEventNotifier::AllDownloadEventNotifier(
    SimpleDownloadManagerCoordinator* simple_download_manager_coordinator)
    : simple_download_manager_coordinator_(simple_download_manager_coordinator),
      download_initialized_(false) {
  simple_download_manager_coordinator_->AddObserver(this);
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  simple_download_manager_coordinator_->GetAllDownloads(&downloads);
  for (download::DownloadItem* download : downloads) {
    download->AddObserver(this);
    observing_.insert(download);
  }
}

AllDownloadEventNotifier::~AllDownloadEventNotifier() {
  if (simple_download_manager_coordinator_)
    simple_download_manager_coordinator_->RemoveObserver(this);
  for (auto it = observing_.begin(); it != observing_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  observing_.clear();

  CHECK(!SimpleDownloadManagerCoordinator::Observer::IsInObserverList());
}

void AllDownloadEventNotifier::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  if (download_initialized_) {
    observer->OnDownloadsInitialized(
        simple_download_manager_coordinator_,
        !simple_download_manager_coordinator_->has_all_history_downloads());
  }
}

void AllDownloadEventNotifier::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AllDownloadEventNotifier::OnDownloadsInitialized(
    bool active_downloads_only) {
  download_initialized_ = true;
  for (auto& observer : observers_)
    observer.OnDownloadsInitialized(simple_download_manager_coordinator_,
                                    active_downloads_only);
}

void AllDownloadEventNotifier::OnManagerGoingDown(
    SimpleDownloadManagerCoordinator* manager) {
  DCHECK_EQ(manager, simple_download_manager_coordinator_);
  for (auto& observer : observers_)
    observer.OnManagerGoingDown(simple_download_manager_coordinator_);
  simple_download_manager_coordinator_->RemoveObserver(this);
  simple_download_manager_coordinator_ = nullptr;
}

void AllDownloadEventNotifier::OnDownloadCreated(DownloadItem* item) {
  if (observing_.find(item) != observing_.end())
    return;
  item->AddObserver(this);
  observing_.insert(item);
  for (auto& observer : observers_)
    observer.OnDownloadCreated(simple_download_manager_coordinator_, item);
}

void AllDownloadEventNotifier::OnDownloadUpdated(DownloadItem* item) {
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(simple_download_manager_coordinator_, item);
}

void AllDownloadEventNotifier::OnDownloadOpened(DownloadItem* item) {
  for (auto& observer : observers_)
    observer.OnDownloadOpened(simple_download_manager_coordinator_, item);
}

void AllDownloadEventNotifier::OnDownloadRemoved(DownloadItem* item) {
  for (auto& observer : observers_)
    observer.OnDownloadRemoved(simple_download_manager_coordinator_, item);
}

void AllDownloadEventNotifier::OnDownloadDestroyed(DownloadItem* item) {
  item->RemoveObserver(this);
  observing_.erase(item);
}

}  // namespace download
