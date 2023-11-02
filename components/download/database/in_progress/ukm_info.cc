// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/in_progress/ukm_info.h"

namespace download {

UkmInfo::UkmInfo() = default;

UkmInfo::UkmInfo(const UkmInfo& other) = default;

UkmInfo::UkmInfo(DownloadSource download_source, int64_t ukm_download_id)
    : download_source(download_source), ukm_download_id(ukm_download_id) {}

UkmInfo::~UkmInfo() = default;

bool UkmInfo::operator==(const UkmInfo& other) const {
  return download_source == other.download_source &&
         ukm_download_id == other.ukm_download_id;
}

}  // namespace download
