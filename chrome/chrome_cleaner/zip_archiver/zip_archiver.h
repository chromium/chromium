// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ZIP_ARCHIVER_ZIP_ARCHIVER_H_
#define CHROME_CHROME_CLEANER_ZIP_ARCHIVER_ZIP_ARCHIVER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/chrome_cleaner/mojom/zip_archiver.mojom.h"

namespace chrome_cleaner {

class ZipArchiver {
 public:
  using ArchiveResultCallback =
      base::OnceCallback<void(mojom::ZipArchiverResultCode)>;

  virtual ~ZipArchiver() = default;

  virtual void Archive(const base::FilePath& src_file_path,
                       ArchiveResultCallback result_callback) = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ZIP_ARCHIVER_ZIP_ARCHIVER_H_
