// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/simple_download_manager_coordinator.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/download/public/common/all_download_event_notifier.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/simple_download_manager.h"

namespace download {

SimpleDownloadManagerCoordinator::SimpleDownloadManagerCoordinator(
    const DownloadWhenFullManagerStartsCallBack&
        download_when_full_manager_starts_cb)
    : simple_download_manager_(nullptr),
      has_all_history_downloads_(false),
      current_manager_has_all_history_downloads_(false),
      initialized_(false),
      download_when_full_manager_starts_cb_(
          download_when_full_manager_starts_cb) {}

SimpleDownloadManagerCoordinator::~SimpleDownloadManagerCoordinator() {
  if (simple_download_manager_)
    simple_download_manager_->RemoveObserver(this);
  for (auto& observer : observers_)
    observer.OnManagerGoingDown(this);
}

void SimpleDownloadManagerCoordinator::SetSimpleDownloadManager(
    SimpleDownloadManager* simple_download_manager,
    bool manages_all_history_downloads) {
  DCHECK(simple_download_manager);
  // Make sure we won't transition from a full manager to a in-progress manager,
  DCHECK(!current_manager_has_all_history_downloads_ ||
         manages_all_history_downloads);

  if (simple_download_manager_)
    simple_download_manager_->RemoveObserver(this);
  current_manager_has_all_history_downloads_ = manages_all_history_downloads;
  simple_download_manager_ = simple_download_manager;
  simple_download_manager_->AddObserver(this);
}

void SimpleDownloadManagerCoordinator::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (initialized_)
    observer->OnDownloadsInitialized(!has_all_history_downloads_);
}

void SimpleDownloadManagerCoordinator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SimpleDownloadManagerCoordinator::DownloadUrl(
    std::unique_ptr<DownloadUrlParameters> parameters) {
  bool result = simple_download_manager_
                    ? simple_download_manager_->CanDownload(parameters.get())
                    : false;
  if (result) {
    simple_download_manager_->DownloadUrl(std::move(parameters));
    return;
  }

  if (!current_manager_has_all_history_downloads_)
    download_when_full_manager_starts_cb_.Run(std::move(parameters));
}

void SimpleDownloadManagerCoordinator::GetAllDownloads(
    std::vector<raw_ptr<DownloadItem, VectorExperimental>>* downloads) {
  if (simple_download_manager_) {
    simple_download_manager_->GetAllDownloads(downloads);
    simple_download_manager_->GetUninitializedActiveDownloadsIfAny(downloads);
  }
}

DownloadItem* SimpleDownloadManagerCoordinator::GetDownloadByGuid(
    const std::string& guid) {
  if (simple_download_manager_)
    return simple_download_manager_->GetDownloadByGuid(guid);
  return nullptr;
}

void SimpleDownloadManagerCoordinator::OnDownloadsInitialized() {
  initialized_ = true;
  has_all_history_downloads_ = current_manager_has_all_history_downloads_;
  for (auto& observer : observers_)
    observer.OnDownloadsInitialized(!has_all_history_downloads_);
}

void SimpleDownloadManagerCoordinator::OnManagerGoingDown() {
  simple_download_manager_ = nullptr;
}

void SimpleDownloadManagerCoordinator::OnDownloadCreated(DownloadItem* item) {
  for (auto& observer : observers_)
    observer.OnDownloadCreated(item);
}

AllDownloadEventNotifier* SimpleDownloadManagerCoordinator::GetNotifier() {
  if (!notifier_)
    notifier_ = std::make_unique<AllDownloadEventNotifier>(this);
  return notifier_.get();
}

void SimpleDownloadManagerCoordinator::CheckForExternallyRemovedDownloads() {
  simple_download_manager_->CheckForHistoryFilesRemoval();
}

}  // namespace download
