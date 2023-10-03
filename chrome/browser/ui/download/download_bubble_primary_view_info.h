// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_PRIMARY_VIEW_INFO_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_PRIMARY_VIEW_INFO_H_

#include "chrome/browser/ui/download/download_bubble_info.h"
#include "chrome/browser/ui/download/download_bubble_row_list_view_info.h"

// Info class for DownloadBubblePrimaryView
class DownloadBubblePrimaryViewInfo
    : public DownloadBubbleInfo<DownloadBubbleInfoChangeObserver> {
 public:
  explicit DownloadBubblePrimaryViewInfo(
      std::vector<DownloadUIModel::DownloadUIModelPtr> models);
  ~DownloadBubblePrimaryViewInfo();

  const DownloadBubbleRowListViewInfo& row_list_view_info() const {
    return row_list_view_info_;
  }

 private:
  DownloadBubbleRowListViewInfo row_list_view_info_;
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_PRIMARY_VIEW_INFO_H_
