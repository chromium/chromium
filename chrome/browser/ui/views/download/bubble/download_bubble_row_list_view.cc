// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"

#include "base/containers/contains.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/scroll_view.h"

using offline_items_collection::ContentId;

DownloadBubbleRowListView::DownloadBubbleRowListView() {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetNotifyEnterExitOnChild(true);
}

DownloadBubbleRowListView::~DownloadBubbleRowListView() = default;

void DownloadBubbleRowListView::AddRow(
    std::unique_ptr<DownloadBubbleRowView> row) {
  const ContentId& id = row->model()->GetContentId();
  CHECK(!base::Contains(rows_by_id_, id));
  auto* child = AddChildView(std::move(row));
  rows_by_id_[id] = child;
}

std::unique_ptr<DownloadBubbleRowView> DownloadBubbleRowListView::RemoveRow(
    DownloadBubbleRowView* row) {
  // We can't remove the row by ContentId here, because by this point the model
  // has nulled out the DownloadItem* and we can no longer retrieve the proper
  // ContentId from `row->model()`.
  for (auto it = rows_by_id_.begin(); it != rows_by_id_.end(); ++it) {
    if (it->second == row) {
      rows_by_id_.erase(it);
      break;
    }
  }
  return RemoveChildViewT(row);
}

DownloadBubbleRowView* DownloadBubbleRowListView::GetRow(
    const offline_items_collection::ContentId& id) const {
  if (const auto it = rows_by_id_.find(id); it != rows_by_id_.end()) {
    return it->second;
  }
  return nullptr;
}

size_t DownloadBubbleRowListView::NumRows() const {
  size_t num_rows = children().size();
  CHECK_EQ(num_rows, rows_by_id_.size());
  return num_rows;
}

BEGIN_METADATA(DownloadBubbleRowListView, views::FlexLayoutView)
END_METADATA
