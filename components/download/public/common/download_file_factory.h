// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_FACTORY_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_FACTORY_H_

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/input_stream.h"
#include "url/gurl.h"

namespace download {
class DownloadDestinationObserver;
class DownloadFile;
struct DownloadSaveInfo;

class COMPONENTS_DOWNLOAD_EXPORT DownloadFileFactory {
 public:
  virtual ~DownloadFileFactory();

  virtual DownloadFile* CreateFile(
      std::unique_ptr<DownloadSaveInfo> save_info,
      const base::FilePath& default_downloads_directory,
      std::unique_ptr<InputStream> stream,
      uint32_t download_id,
      const base::FilePath& duplicate_download_file_path,
      base::WeakPtr<DownloadDestinationObserver> observer);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_FACTORY_H_
