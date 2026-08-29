// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/simple_download_manager.h"

#include <utility>

#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"

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
  std::vector<base::OnceClosure> callbacks =
      std::move(active_downloads_initialized_callbacks_);
  for (auto& callback : callbacks) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
}

void SimpleDownloadManager::OnNewDownloadCreated(DownloadItem* download) {
  for (auto& observer : simple_download_manager_observers_)
    observer.OnDownloadCreated(download);
}

void SimpleDownloadManager::NotifyInitialized() {
  for (auto& observer : simple_download_manager_observers_)
    observer.OnDownloadsInitialized();
}

void SimpleDownloadManager::WaitForActiveDownloadsInitialization(
    base::OnceClosure callback) {
  if (callback.is_null()) {
    return;
  }
  if (initialized_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }
  active_downloads_initialized_callbacks_.push_back(std::move(callback));
}

}  // namespace download
