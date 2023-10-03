// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_VIEW_INFO_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_VIEW_INFO_H_

#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_bubble_info.h"

namespace offline_item_collection {
class ContentId;
}

// Interface for observers of changes to a download row
class DownloadBubbleRowViewInfoObserver : public base::CheckedObserver {
 public:
  DownloadBubbleRowViewInfoObserver();
  ~DownloadBubbleRowViewInfoObserver() override;

  virtual void OnInfoChanged() {}
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

 private:
  // Overrides DownloadUIModel::Delegate:
  void OnDownloadOpened() override;
  void OnDownloadUpdated() override;
  void OnDownloadDestroyed(
      const offline_items_collection::ContentId& id) override;

  DownloadUIModel::DownloadUIModelPtr model_;
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_VIEW_INFO_H_
