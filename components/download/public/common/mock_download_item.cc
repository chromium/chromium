// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/mock_download_item.h"

namespace download {

MockDownloadItem::MockDownloadItem() = default;

MockDownloadItem::~MockDownloadItem() {
  for (auto& observer : observers_)
    observer.OnDownloadDestroyed(this);
}

void MockDownloadItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockDownloadItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MockDownloadItem::NotifyObserversDownloadOpened() {
  for (auto& observer : observers_)
    observer.OnDownloadOpened(this);
}

void MockDownloadItem::NotifyObserversDownloadRemoved() {
  for (auto& observer : observers_)
    observer.OnDownloadRemoved(this);
}

void MockDownloadItem::NotifyObserversDownloadUpdated() {
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
}

}  // namespace download
