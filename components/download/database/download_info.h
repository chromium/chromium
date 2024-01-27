// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_INFO_H_
#define COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_INFO_H_

#include <optional>
#include <string>

#include "components/download/database/in_progress/in_progress_info.h"
#include "components/download/database/in_progress/ukm_info.h"

namespace download {

// Contains needed information to reconstruct a download item.
struct DownloadInfo {
 public:
  DownloadInfo();
  DownloadInfo(const DownloadInfo& other);
  ~DownloadInfo();

  bool operator==(const DownloadInfo& other) const;

  // Download GUID.
  std::string guid;

  // Download ID.
  // Deprecated, only kept for the purpose of download extension API.
  int id = -1;

  // UKM information for reporting.
  std::optional<UkmInfo> ukm_info;

  // In progress information for active download.
  std::optional<InProgressInfo> in_progress_info;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_INFO_H_
