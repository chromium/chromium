// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/decoder/downloader_impl.h"

namespace chromeos {
namespace ime {

DownloaderImpl::DownloaderImpl() {
  // TODO(crbug/946913): Implement this.
}

DownloaderImpl::~DownloaderImpl() {
  // TODO(crbug/946913): Implement this.
}

int DownloaderImpl::DownloadToFile(const char* url,
                                   const DownloadOptions& options,
                                   const char* file_path,
                                   ImeCrosDownloadCallback callback) {
  // TODO(crbug/946913): Implement this.
  return 0;
}

void DownloaderImpl::Cancel(int request_id) {
  // TODO(crbug/946913): Implement this.
}

}  // namespace ime
}  // namespace chromeos
