// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_file_factory.h"

#include <utility>

#include "components/download/internal/common/download_file_with_copy.h"
#include "components/download/public/common/download_file_impl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/download/internal/common/in_memory_download_file.h"
#include "components/download/public/common/download_stats.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace download {

DownloadFileFactory::~DownloadFileFactory() = default;

DownloadFile* DownloadFileFactory::CreateFile(
    std::unique_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_downloads_directory,
    std::unique_ptr<InputStream> stream,
    uint32_t download_id,
    const base::FilePath& duplicate_download_file_path,
    base::WeakPtr<DownloadDestinationObserver> observer) {
#if BUILDFLAG(IS_ANDROID)
  if (save_info->use_in_memory_file) {
    return new InMemoryDownloadFile(std::move(save_info), std::move(stream),
                                    observer);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  if (!duplicate_download_file_path.empty()) {
#if BUILDFLAG(IS_ANDROID)
    RecordDuplicatePdfDownloadTriggered(/*open_inline=*/true);
#endif  // BUILDFLAG(IS_ANDROID)
    return new DownloadFileWithCopy(duplicate_download_file_path, observer);
  } else {
    return new DownloadFileImpl(std::move(save_info),
                                default_downloads_directory, std::move(stream),
                                download_id, observer);
  }
}

}  // namespace download
