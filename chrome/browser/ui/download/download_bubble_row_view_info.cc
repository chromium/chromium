// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_row_view_info.h"

DownloadBubbleRowViewInfoObserver::DownloadBubbleRowViewInfoObserver() =
    default;

DownloadBubbleRowViewInfoObserver::~DownloadBubbleRowViewInfoObserver() {
  CHECK(!IsInObserverList());
}

DownloadBubbleRowViewInfo::DownloadBubbleRowViewInfo(
    DownloadUIModel::DownloadUIModelPtr model)
    : model_(std::move(model)) {
  model_->SetDelegate(this);
}

DownloadBubbleRowViewInfo::~DownloadBubbleRowViewInfo() {
  model_->SetDelegate(nullptr);
}

void DownloadBubbleRowViewInfo::OnDownloadOpened() {
  model_->SetActionedOn(true);
}

void DownloadBubbleRowViewInfo::OnDownloadUpdated() {
  NotifyObservers(&DownloadBubbleRowViewInfoObserver::OnInfoChanged);
}

void DownloadBubbleRowViewInfo::OnDownloadDestroyed(
    const offline_items_collection::ContentId& id) {
  NotifyObservers(&DownloadBubbleRowViewInfoObserver::OnDownloadDestroyed, id);
}
