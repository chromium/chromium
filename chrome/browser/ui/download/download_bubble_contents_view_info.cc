// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_contents_view_info.h"

#include "chrome/browser/ui/download/download_bubble_row_view_info.h"

DownloadBubbleContentsViewInfo::DownloadBubbleContentsViewInfo(
    std::vector<DownloadUIModel::DownloadUIModelPtr> models)
    : row_list_view_info_(std::move(models)), security_view_info_() {}

DownloadBubbleContentsViewInfo::~DownloadBubbleContentsViewInfo() = default;

DownloadUIModel* DownloadBubbleContentsViewInfo::GetDownloadModel(
    const ContentId& id) const {
  if (const DownloadBubbleRowViewInfo* row_info =
          row_list_view_info_.GetRowInfo(id);
      row_info) {
    return row_info->model();
  }

  return nullptr;
}

void DownloadBubbleContentsViewInfo::InitializeSecurityView(
    const ContentId& id) {
  CHECK(id != ContentId());
  if (security_view_info_.content_id() == id) {
    return;
  }
  DownloadUIModel* model = GetDownloadModel(id);
  CHECK(model);
  security_view_info_.InitializeForDownload(*model);
}

void DownloadBubbleContentsViewInfo::ResetSecurityView() {
  security_view_info_.Reset();
}
