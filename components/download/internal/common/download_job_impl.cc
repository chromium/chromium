// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/download_job_impl.h"

namespace download {

DownloadJobImpl::DownloadJobImpl(DownloadItem* download_item,
                                 CancelRequestCallback cancel_request_callback,
                                 bool is_parallizable)
    : DownloadJob(download_item, std::move(cancel_request_callback)),
      is_parallizable_(is_parallizable) {}

DownloadJobImpl::~DownloadJobImpl() = default;

bool DownloadJobImpl::IsParallelizable() const {
  return is_parallizable_;
}

}  // namespace download
