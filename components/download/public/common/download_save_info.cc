// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_save_info.h"

namespace download {

// static
const int64_t DownloadSaveInfo::kLengthFullContent = 0;

DownloadSaveInfo::DownloadSaveInfo() = default;

DownloadSaveInfo::~DownloadSaveInfo() = default;

DownloadSaveInfo::DownloadSaveInfo(DownloadSaveInfo&& that) = default;

int64_t DownloadSaveInfo::GetStartingFileWriteOffset() const {
  return file_offset >= 0 ? file_offset : offset;
}

bool DownloadSaveInfo::IsArbitraryRangeRequest() const {
  return range_request_from != kInvalidRange ||
         range_request_to != kInvalidRange;
}

}  // namespace download
