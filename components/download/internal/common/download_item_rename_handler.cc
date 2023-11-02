// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_item_rename_handler.h"

#include "base/files/file_path.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"

namespace download {

DownloadItemRenameHandler::DownloadItemRenameHandler(
    DownloadItem* download_item)
    : download_item_(download_item) {}

DownloadItemRenameHandler::~DownloadItemRenameHandler() = default;

}  // namespace download
