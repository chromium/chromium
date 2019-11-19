// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/save_package_download_job.h"

namespace download {

SavePackageDownloadJob::SavePackageDownloadJob(
    DownloadItem* download_item,
    DownloadJob::CancelRequestCallback cancel_request_callback)
    : DownloadJob(download_item, std::move(cancel_request_callback)) {}

SavePackageDownloadJob::~SavePackageDownloadJob() = default;

bool SavePackageDownloadJob::IsSavePackageDownload() const {
  return true;
}

}  // namespace download
