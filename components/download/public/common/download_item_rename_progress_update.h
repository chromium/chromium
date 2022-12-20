// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_PROGRESS_UPDATE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_PROGRESS_UPDATE_H_

#include "base/files/file_path.h"

namespace download {

struct DownloadItemRenameProgressUpdate {
  // Only used in DownloadItemImpl to update the corresponding field:
  base::FilePath target_file_name;  // destination_info_.target_path.
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_PROGRESS_UPDATE_H_
