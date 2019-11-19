// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_save_info.h"

namespace download {

// static
const int64_t DownloadSaveInfo::kLengthFullContent = 0;

DownloadSaveInfo::DownloadSaveInfo() = default;

DownloadSaveInfo::~DownloadSaveInfo() = default;

DownloadSaveInfo::DownloadSaveInfo(DownloadSaveInfo&& that) = default;

int64_t DownloadSaveInfo::GetStartingFileWriteOffset() {
  return file_offset >= 0 ? file_offset : offset;
}

}  // namespace download
