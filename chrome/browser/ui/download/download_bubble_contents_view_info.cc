// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_contents_view_info.h"

DownloadBubbleContentsViewInfo::DownloadBubbleContentsViewInfo(
    std::vector<DownloadUIModel::DownloadUIModelPtr> models)
    : row_list_view_info_(std::move(models)) {}

DownloadBubbleContentsViewInfo::~DownloadBubbleContentsViewInfo() = default;
