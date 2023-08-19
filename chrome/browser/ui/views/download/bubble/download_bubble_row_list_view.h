// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

class DownloadBubbleRowView;

class DownloadBubbleRowListView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(DownloadBubbleRowListView);

  DownloadBubbleRowListView();
  ~DownloadBubbleRowListView() override;
  DownloadBubbleRowListView(const DownloadBubbleRowListView&) = delete;
  DownloadBubbleRowListView& operator=(const DownloadBubbleRowListView&) =
      delete;

  // TODO(crbug.com/1344515): Add functionality for adding a new download while
  // this is already open.

  // Adds a row to the bottom of the list.
  void AddRow(std::unique_ptr<DownloadBubbleRowView> row);

  // Removes a row and updates the `rows_by_id_` map. Returns ownership of the
  // row to the caller.
  std::unique_ptr<DownloadBubbleRowView> RemoveRow(DownloadBubbleRowView* row);

  // Returns the number of rows.
  size_t NumRows() const;

 private:
  // Map of download item's ID to child view in the row list.
  std::map<offline_items_collection::ContentId, DownloadBubbleRowView*>
      rows_by_id_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_
