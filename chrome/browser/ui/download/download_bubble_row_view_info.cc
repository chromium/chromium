// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_row_view_info.h"

#include "chrome/browser/ui/download/download_item_mode.h"

DownloadBubbleRowViewInfoObserver::DownloadBubbleRowViewInfoObserver() =
    default;

DownloadBubbleRowViewInfoObserver::~DownloadBubbleRowViewInfoObserver() {
  CHECK(!IsInObserverList());
}

DownloadBubbleRowViewInfo::DownloadBubbleRowViewInfo(
    DownloadUIModel::DownloadUIModelPtr model)
    : model_(std::move(model)),
      mode_(download::GetDesiredDownloadItemMode(model_.get())),
      state_(model_->GetState()),
      is_paused_(model_->IsPaused()) {
  model_->SetDelegate(this);
}

DownloadBubbleRowViewInfo::~DownloadBubbleRowViewInfo() {
  model_->SetDelegate(nullptr);
}

void DownloadBubbleRowViewInfo::OnDownloadOpened() {
  model_->SetActionedOn(true);
}

void DownloadBubbleRowViewInfo::OnDownloadUpdated() {
  if (state_ != model_->GetState()) {
    NotifyObservers(&DownloadBubbleRowViewInfoObserver::OnDownloadStateChanged,
                    state_, model_->GetState());
  }

  mode_ = download::GetDesiredDownloadItemMode(model_.get());
  state_ = model_->GetState();
  is_paused_ = model_->IsPaused();
  NotifyObservers(&DownloadBubbleRowViewInfoObserver::OnInfoChanged);
}

void DownloadBubbleRowViewInfo::OnDownloadDestroyed(
    const offline_items_collection::ContentId& id) {
  NotifyObservers(&DownloadBubbleRowViewInfoObserver::OnDownloadDestroyed, id);
}
