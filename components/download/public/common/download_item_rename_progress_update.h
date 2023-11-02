// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_PROGRESS_UPDATE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_PROGRESS_UPDATE_H_

#include "base/files/file_path.h"
#include "components/enterprise/common/download_item_reroute_info.h"

namespace download {

using enterprise_connectors::DownloadItemRerouteInfo;

struct DownloadItemRenameProgressUpdate {
  // Only used in DownloadItemImpl to update the corresponding field:
  base::FilePath target_file_name;  // destination_info_.target_path.

  // Reroute info to be stored into / read from databases.
  // This is a proto because the in-progress DB already can
  // store proto, but the history DB can only store string, so will need to be
  // serialized.
  DownloadItemRerouteInfo reroute_info;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_PROGRESS_UPDATE_H_
