// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_REQUEST_HANDLE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_REQUEST_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/url_download_handler.h"

namespace download {

// Implementation of the DownloadRequestHandleInterface to handle url download.
class COMPONENTS_DOWNLOAD_EXPORT UrlDownloadRequestHandle {
 public:
  UrlDownloadRequestHandle(
      base::WeakPtr<UrlDownloadHandler> downloader,
      scoped_refptr<base::SequencedTaskRunner> downloader_task_runner);
  UrlDownloadRequestHandle(UrlDownloadRequestHandle&& other);
  UrlDownloadRequestHandle& operator=(UrlDownloadRequestHandle&& other);
  ~UrlDownloadRequestHandle();

 private:
  base::WeakPtr<UrlDownloadHandler> downloader_;
  scoped_refptr<base::SequencedTaskRunner> downloader_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UrlDownloadRequestHandle);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_REQUEST_HANDLE_H_
