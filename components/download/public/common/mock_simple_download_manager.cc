// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/mock_simple_download_manager.h"

namespace download {

MockSimpleDownloadManager::MockSimpleDownloadManager() = default;

MockSimpleDownloadManager::~MockSimpleDownloadManager() = default;

void MockSimpleDownloadManager::NotifyOnDownloadInitialized() {
  OnInitialized();
}

void MockSimpleDownloadManager::NotifyOnNewDownloadCreated(DownloadItem* item) {
  for (auto& observer : simple_download_manager_observers_)
    observer.OnDownloadCreated(item);
}

const DownloadUrlParameters*
MockSimpleDownloadManager::GetDownloadUrlParameters() {
  DCHECK(params_.get());
  return params_.get();
}

void MockSimpleDownloadManager::DownloadUrl(
    std::unique_ptr<DownloadUrlParameters> params) {
  DCHECK(params.get());
  DownloadUrlMock(params.get());
  params_ = std::move(params);
}

}  // namespace download
