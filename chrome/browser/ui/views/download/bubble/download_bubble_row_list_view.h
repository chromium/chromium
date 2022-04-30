// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

class DownloadBubbleRowListView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(DownloadBubbleRowListView);

  explicit DownloadBubbleRowListView(bool is_partial_view);
  ~DownloadBubbleRowListView() override;
  DownloadBubbleRowListView(const DownloadBubbleRowListView&) = delete;
  DownloadBubbleRowListView& operator=(const DownloadBubbleRowListView&) =
      delete;

 private:
  bool is_partial_view_;
  base::Time creation_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_
