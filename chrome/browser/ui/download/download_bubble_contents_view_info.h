// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_CONTENTS_VIEW_INFO_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_CONTENTS_VIEW_INFO_H_

#include "chrome/browser/ui/download/download_bubble_info.h"
#include "chrome/browser/ui/download/download_bubble_row_list_view_info.h"
#include "chrome/browser/ui/download/download_bubble_security_view_info.h"

// Info class for DownloadBubbleContentsView
class DownloadBubbleContentsViewInfo
    : public DownloadBubbleInfo<DownloadBubbleInfoChangeObserver> {
 public:
  explicit DownloadBubbleContentsViewInfo(
      std::vector<DownloadUIModel::DownloadUIModelPtr> models);
  ~DownloadBubbleContentsViewInfo();

  const DownloadBubbleRowListViewInfo& row_list_view_info() const {
    return row_list_view_info_;
  }

  const DownloadBubbleSecurityViewInfo& security_view_info() const {
    return security_view_info_;
  }

  // Get the `DownloadUIModel` for any `id` among those listed within
  // the row list view.
  DownloadUIModel* GetDownloadModel(const ContentId& id) const;

  void InitializeSecurityView(const ContentId& id);
  void ResetSecurityView();

  // Updates the internal models, adding new ones and removing those no longer
  // present.
  void UpdateModels(std::vector<DownloadUIModel::DownloadUIModelPtr> models);

 private:
  DownloadBubbleRowListViewInfo row_list_view_info_;
  DownloadBubbleSecurityViewInfo security_view_info_;
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_CONTENTS_VIEW_INFO_H_
