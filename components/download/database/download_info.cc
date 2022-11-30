// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/download_info.h"

namespace download {

DownloadInfo::DownloadInfo() = default;

DownloadInfo::DownloadInfo(const DownloadInfo& other) = default;

DownloadInfo::~DownloadInfo() = default;

bool DownloadInfo::operator==(const DownloadInfo& other) const {
  return guid == other.guid && id == other.id && ukm_info == other.ukm_info &&
         in_progress_info == other.in_progress_info;
}

}  // namespace download
