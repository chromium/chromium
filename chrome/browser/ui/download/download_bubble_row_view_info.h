// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_VIEW_INFO_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_VIEW_INFO_H_

#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_bubble_info.h"
#include "chrome/browser/ui/download/download_item_mode.h"

namespace offline_item_collection {
class ContentId;
}

// Interface for observers of changes to a download row
class DownloadBubbleRowViewInfoObserver : public base::CheckedObserver {
 public:
  DownloadBubbleRowViewInfoObserver();
  ~DownloadBubbleRowViewInfoObserver() override;

  // Called whenever the info changes
  virtual void OnInfoChanged() {}

  // Called when the download changes state.
  virtual void OnDownloadStateChanged(
      download::DownloadItem::DownloadState old_state,
      download::DownloadItem::DownloadState new_state) {}

  // Called when the download is destroyed.
  virtual void OnDownloadDestroyed(
      const offline_items_collection::ContentId& id) {}
};

// Info class for a DownloadBubbleRowView
class DownloadBubbleRowViewInfo
    : public DownloadBubbleInfo<DownloadBubbleRowViewInfoObserver>,
      public DownloadUIModel::Delegate {
 public:
  explicit DownloadBubbleRowViewInfo(DownloadUIModel::DownloadUIModelPtr model);
  ~DownloadBubbleRowViewInfo() override;

  DownloadUIModel* model() const { return model_.get(); }
  download::DownloadItemMode mode() const { return mode_; }

 private:
  // Overrides DownloadUIModel::Delegate:
  void OnDownloadOpened() override;
  void OnDownloadUpdated() override;
  void OnDownloadDestroyed(
      const offline_items_collection::ContentId& id) override;

  DownloadUIModel::DownloadUIModelPtr model_;

  // Cached attributes of the model. This helps filter when we have to update
  // the other fields.
  download::DownloadItemMode mode_ = download::DownloadItemMode::kNormal;
  download::DownloadItem::DownloadState state_ =
      download::DownloadItem::IN_PROGRESS;
  bool is_paused_ = false;
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_VIEW_INFO_H_
