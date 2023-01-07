// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_SAVE_PACKAGE_DOWNLOAD_JOB_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_SAVE_PACKAGE_DOWNLOAD_JOB_H_

#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_job.h"

namespace download {

class SavePackageDownloadJob : public DownloadJob {
 public:
  SavePackageDownloadJob(
      DownloadItem* download_item,
      DownloadJob::CancelRequestCallback cancel_request_callback);

  SavePackageDownloadJob(const SavePackageDownloadJob&) = delete;
  SavePackageDownloadJob& operator=(const SavePackageDownloadJob&) = delete;

  ~SavePackageDownloadJob() override;

  // DownloadJob implementation.
  bool IsSavePackageDownload() const override;
};

}  //  namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_SAVE_PACKAGE_DOWNLOAD_JOB_H_
