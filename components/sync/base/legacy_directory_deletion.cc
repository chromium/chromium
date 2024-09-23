// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/legacy_directory_deletion.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace syncer {

// Delete the directory database files from the sync data folder to cleanup
// all files. The main purpose is to delete the legacy Directory files (sqlite)
// but it also currently deletes the files corresponding to the modern
// NigoriStorageImpl.
void DeleteLegacyDirectoryFilesAndNigoriStorage(
    const base::FilePath& directory_path) {
  // We assume that the directory database files are all top level files, and
  // use no folders. We also assume that there might be child folders under
  // |directory_path| that are used for non-directory things, like storing
  // DataTypeStore/LevelDB data, and we expressly do not want to delete those.
  if (!base::DirectoryExists(directory_path)) {
    return;
  }

  base::FileEnumerator fe(directory_path, false, base::FileEnumerator::FILES);
  for (base::FilePath current = fe.Next(); !current.empty();
       current = fe.Next()) {
    if (!base::DeleteFile(current)) {
      DLOG(ERROR) << "Could not delete all sync directory files.";
    }
  }
}

}  // namespace syncer
