// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_LEGACY_DIRECTORY_DELETION_H_
#define COMPONENTS_SYNC_BASE_LEGACY_DIRECTORY_DELETION_H_

#include "base/files/file_path.h"

namespace syncer {

// Delete the directory database files from the sync data folder to cleanup
// all files. The main purpose is to delete the legacy Directory files (sqlite)
// but it also currently deletes the files corresponding to the modern
// NigoriStorageImpl.
void DeleteLegacyDirectoryFilesAndNigoriStorage(
    const base::FilePath& directory_path);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_LEGACY_DIRECTORY_DELETION_H_
