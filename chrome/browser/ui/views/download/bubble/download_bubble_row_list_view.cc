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

DownloadBubbleRowListView::DownloadBubbleRowListView(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<DownloadBubbleUIController> bubble_controller,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    int fixed_width,
    const DownloadBubbleRowListViewInfo& info,
    bool is_in_partial_view)
    : browser_(browser),
      bubble_controller_(bubble_controller),
      navigation_handler_(navigation_handler),
      fixed_width_(fixed_width),
      is_in_partial_view_(is_in_partial_view),
      info_(info) {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetNotifyEnterExitOnChild(true);
  info_->AddObserver(this);

  for (const DownloadBubbleRowViewInfo& row_info : info_->rows()) {
    AddRow(row_info);
  }
}

DownloadBubbleRowListView::~DownloadBubbleRowListView() {
  info_->RemoveObserver(this);
}

void DownloadBubbleRowListView::AddRow(
    const DownloadBubbleRowViewInfo& row_info) {
  const ContentId& id = row_info.model()->GetContentId();
  CHECK(!base::Contains(rows_by_id_, id));
  auto* child = AddChildView(std::make_unique<DownloadBubbleRowView>(
      row_info, bubble_controller_, navigation_handler_, browser_, fixed_width_,
      is_in_partial_view_));
  rows_by_id_[id] = child;
}

std::unique_ptr<DownloadBubbleRowView> DownloadBubbleRowListView::RemoveRow(
    DownloadBubbleRowView* row) {
  CHECK(row);
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

void DownloadBubbleRowListView::OnRowAdded(
    const offline_items_collection::ContentId& id) {
  const DownloadBubbleRowViewInfo* row_info = info_->GetRowInfo(id);
  CHECK(row_info);
  AddRow(*row_info);
}

void DownloadBubbleRowListView::OnRowWillBeRemoved(
    const offline_items_collection::ContentId& id) {
  RemoveRow(GetRow(id));
}

BEGIN_METADATA(DownloadBubbleRowListView)
END_METADATA
