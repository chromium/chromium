// Copyright 2019 The Chromium Authors. All rights reserved.
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

}  // namespace download
