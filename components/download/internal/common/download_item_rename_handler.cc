// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_item_rename_handler.h"

#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"

namespace download {

DownloadItemRenameHandler::DownloadItemRenameHandler() = default;

DownloadItemRenameHandler::~DownloadItemRenameHandler() = default;

void DownloadItemRenameHandler::Start(DownloadItem* download_item,
                                      Callback callback) {
  std::move(callback).Run(DOWNLOAD_INTERRUPT_REASON_NONE,
                          download_item->GetTargetFilePath());
}

}  // namespace download
