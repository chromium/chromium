// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace ash::file_manager {

// A file indexing service. The main task of this service is to efficiently
// associate terms with files. Instead of using files directly, we rely on
// the FileInfo class, which stores file's URL, size and modification time.
// Terms are pairs of field:text, where field identifies where the text is
// coming from. For example, if text is derived from the files content, the
// field can be "content". if the text is a label added to the file, the field
// could be "label".
class COMPONENT_EXPORT(FILE_MANAGER) FileIndexService {
 public:
  explicit FileIndexService(base::FilePath profile_path);
  ~FileIndexService();

 private:
  base::FilePath profile_path_;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_
