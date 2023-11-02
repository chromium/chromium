// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_UKM_INFO_H_
#define COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_UKM_INFO_H_

#include <stdint.h>

#include "components/download/public/common/download_source.h"

namespace download {

// Contains information for UKM reporting.
struct UkmInfo {
 public:
  UkmInfo();
  UkmInfo(DownloadSource download_source, int64_t ukm_download_id);
  UkmInfo(const UkmInfo& other);
  ~UkmInfo();

  bool operator==(const UkmInfo& other) const;

  // The source that triggered the download.
  DownloadSource download_source = DownloadSource::UNKNOWN;

  // Unique ID that tracks the download UKM entry, where 0 means the
  // download_id is not yet initialized.
  uint64_t ukm_download_id = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_UKM_INFO_H_
