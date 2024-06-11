// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_item_rename_handler.h"

#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"

namespace download {

DownloadItemRenameHandler::DownloadItemRenameHandler(
    DownloadItem* download_item)
    : download_item_(download_item) {}

DownloadItemRenameHandler::~DownloadItemRenameHandler() = default;

void DownloadItemRenameHandler::Start(ProgressCallback progress_callback,
                                      RenameCallback rename_callback) {
  std::move(rename_callback)
      .Run(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, base::FilePath());
}

bool DownloadItemRenameHandler::ShowRenameProgress() {
  return false;
}

}  // namespace download
