// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_contents_view_info.h"

#include <algorithm>

#include "chrome/browser/ui/download/download_bubble_row_view_info.h"

DownloadBubbleContentsViewInfo::DownloadBubbleContentsViewInfo(
    std::vector<DownloadUIModel::DownloadUIModelPtr> models)
    : row_list_view_info_(std::move(models)) {}

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

void DownloadBubbleContentsViewInfo::UpdateModels(
    std::vector<DownloadUIModel::DownloadUIModelPtr> models) {
  std::vector<offline_items_collection::ContentId> to_remove;
  for (const auto& row : row_list_view_info_.rows()) {
    const offline_items_collection::ContentId& id = row.model()->GetContentId();
    if (!std::ranges::any_of(
            models, [&id](const auto& m) { return m->GetContentId() == id; })) {
      to_remove.push_back(id);
    }
  }
  for (const auto& id : to_remove) {
    row_list_view_info_.RemoveRow(id);
  }
  for (size_t i = 0; i < models.size(); ++i) {
    const offline_items_collection::ContentId& id = models[i]->GetContentId();
    if (!row_list_view_info_.GetRowInfo(id)) {
      row_list_view_info_.AddRow(std::move(models[i]), i);
    }
  }
}
