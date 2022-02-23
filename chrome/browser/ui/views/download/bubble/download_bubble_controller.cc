// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_controller.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/download_manager.h"

#include "base/files/file_path.h"

DownloadBubbleUIController::DownloadBubbleUIController(
    content::DownloadManager* manager)
    : download_manager_(manager) {}

void DownloadBubbleUIController::OnManagerGoingDown(
    content::DownloadManager* manager) {
  if (manager == download_manager_) {
    download_manager_ = nullptr;
  }
}

std::unique_ptr<DownloadBubbleRowListView>
DownloadBubbleUIController::GetMainView() {
  auto row_list_view = std::make_unique<DownloadBubbleRowListView>();
  if (!download_manager_)
    return row_list_view;
  std::vector<download::DownloadItem*> download_items;
  download_manager_->GetAllDownloads(&download_items);
  for (download::DownloadItem* item : download_items) {
    base::Time end_time = item->GetEndTime();
    if (end_time.is_null() ||
        ((base::Time::Now() - end_time) <= base::Days(1))) {
      row_list_view->AddChildView(std::make_unique<DownloadBubbleRowView>(
          DownloadItemModel::Wrap(item)));
    }
  }
  return row_list_view;
}
