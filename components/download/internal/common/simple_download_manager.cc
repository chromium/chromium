// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/simple_download_manager.h"
#include "base/observer_list.h"

namespace download {

SimpleDownloadManager::SimpleDownloadManager() = default;

SimpleDownloadManager::~SimpleDownloadManager() {
  for (auto& observer : simple_download_manager_observers_)
    observer.OnManagerGoingDown();
}

void SimpleDownloadManager::AddObserver(Observer* observer) {
  simple_download_manager_observers_.AddObserver(observer);
  if (initialized_)
    NotifyInitialized();
}

void SimpleDownloadManager::RemoveObserver(Observer* observer) {
  simple_download_manager_observers_.RemoveObserver(observer);
}

void SimpleDownloadManager::OnInitialized() {
  initialized_ = true;
  NotifyInitialized();
}

void SimpleDownloadManager::OnNewDownloadCreated(DownloadItem* download) {
  for (auto& observer : simple_download_manager_observers_)
    observer.OnDownloadCreated(download);
}

void SimpleDownloadManager::NotifyInitialized() {
  for (auto& observer : simple_download_manager_observers_)
    observer.OnDownloadsInitialized();
}

}  // namespace download
