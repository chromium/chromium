// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_file_factory.h"

#include <utility>

#include "components/download/public/common/download_file_impl.h"

namespace download {

DownloadFileFactory::~DownloadFileFactory() {}

DownloadFile* DownloadFileFactory::CreateFile(
    std::unique_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_downloads_directory,
    std::unique_ptr<InputStream> stream,
    uint32_t download_id,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  return new DownloadFileImpl(std::move(save_info), default_downloads_directory,
                              std::move(stream), download_id, observer);
}

}  // namespace download
