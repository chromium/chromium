// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/download_blockage_status.h"

namespace download {

DownloadBlockageStatus::DownloadBlockageStatus()
    : blocked_by_criteria(false),
      blocked_by_navigation(false),
      blocked_by_downloads(false),
      entry_not_active(false) {}

DownloadBlockageStatus::~DownloadBlockageStatus() = default;

bool DownloadBlockageStatus::IsBlocked() {
  return blocked_by_criteria || blocked_by_navigation || blocked_by_downloads ||
         entry_not_active;
}

}  // namespace download
