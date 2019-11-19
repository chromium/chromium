// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_ZIP_FILE_CREATOR_H_
#define CHROME_SERVICES_FILE_UTIL_ZIP_FILE_CREATOR_H_

#include <vector>

#include "chrome/services/file_util/public/mojom/zip_file_creator.mojom.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"

namespace base {
class FilePath;
}

namespace chrome {

class ZipFileCreator : public chrome::mojom::ZipFileCreator {
 public:
  ZipFileCreator();
  ~ZipFileCreator() override;

 private:
  // chrome::mojom::ZipFileCreator:
  void CreateZipFile(
      mojo::PendingRemote<filesystem::mojom::Directory> source_dir_remote,
      const base::FilePath& source_dir,
      const std::vector<base::FilePath>& source_relative_paths,
      base::File zip_file,
      CreateZipFileCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(ZipFileCreator);
};

}  // namespace chrome

#endif  // CHROME_SERVICES_FILE_UTIL_ZIP_FILE_CREATOR_H_
