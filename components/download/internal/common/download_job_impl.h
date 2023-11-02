// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_JOB_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_JOB_IMPL_H_

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_job.h"

namespace download {

class DownloadItem;

class COMPONENTS_DOWNLOAD_EXPORT DownloadJobImpl : public DownloadJob {
 public:
  DownloadJobImpl(DownloadItem* download_item,
                  CancelRequestCallback cancel_request_callback,
                  bool is_parallizable);

  DownloadJobImpl(const DownloadJobImpl&) = delete;
  DownloadJobImpl& operator=(const DownloadJobImpl&) = delete;

  ~DownloadJobImpl() override;

  // DownloadJob implementation.
  bool IsParallelizable() const override;

 private:
  // Whether the download can be parallized.
  bool is_parallizable_;
};

}  //  namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_JOB_IMPL_H_
