// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/download_slice_info.h"

#include "components/history/core/browser/download_constants.h"

namespace history {

DownloadSliceInfo::DownloadSliceInfo()
    : download_id(kInvalidDownloadId),
      offset(0),
      received_bytes(0),
      finished(false) {}

DownloadSliceInfo::DownloadSliceInfo(DownloadId download_id,
                                     int64_t offset,
                                     int64_t received_bytes,
                                     bool finished)
    : download_id(download_id),
      offset(offset),
      received_bytes(received_bytes),
      finished(finished) {}

DownloadSliceInfo::DownloadSliceInfo(const DownloadSliceInfo& other) = default;

DownloadSliceInfo::~DownloadSliceInfo() = default;

bool DownloadSliceInfo::operator==(const DownloadSliceInfo& rhs) const {
  return download_id == rhs.download_id && offset == rhs.offset &&
         received_bytes == rhs.received_bytes && finished == rhs.finished;
}

}  // namespace history
