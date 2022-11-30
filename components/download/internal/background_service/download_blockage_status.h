// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_BLOCKAGE_STATUS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_BLOCKAGE_STATUS_H_

namespace download {

// A helper class representing various conditions where a download can be
// blocked.
struct DownloadBlockageStatus {
  bool blocked_by_criteria;
  bool blocked_by_navigation;
  bool blocked_by_downloads;
  bool entry_not_active;

  DownloadBlockageStatus();
  ~DownloadBlockageStatus();

  // Whether the download is blocked currently.
  bool IsBlocked();
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_BLOCKAGE_STATUS_H_
