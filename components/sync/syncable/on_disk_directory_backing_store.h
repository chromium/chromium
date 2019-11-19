// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_ON_DISK_DIRECTORY_BACKING_STORE_H_
#define COMPONENTS_SYNC_SYNCABLE_ON_DISK_DIRECTORY_BACKING_STORE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/sync/syncable/directory_backing_store.h"

namespace syncer {
namespace syncable {

// This is the concrete class that provides a useful implementation of
// DirectoryBackingStore.
class OnDiskDirectoryBackingStore : public DirectoryBackingStore {
 public:
  OnDiskDirectoryBackingStore(
      const std::string& dir_name,
      const base::RepeatingCallback<std::string()>& cache_guid_generator,
      const base::FilePath& backing_file_path);
  ~OnDiskDirectoryBackingStore() override;
  DirOpenResult Load(Directory::MetahandlesMap* handles_map,
                     MetahandleSet* metahandles_to_purge,
                     Directory::KernelLoadInfo* kernel_load_info) override;

 protected:
  // Subclasses may override this to avoid a possible DCHECK.
  virtual void ReportFirstTryOpenFailure();

  // Returns the file path of the back store.
  const base::FilePath& backing_file_path() const;

 private:
  // A helper function that will make one attempt to load the directory.
  // Unlike Load(), it does not attempt to recover from failure.
  DirOpenResult TryLoad(Directory::MetahandlesMap* handles_map,
                        MetahandleSet* metahandles_to_purge,
                        Directory::KernelLoadInfo* kernel_load_info);

  // The path to the sync DB.
  const base::FilePath backing_file_path_;

  DISALLOW_COPY_AND_ASSIGN(OnDiskDirectoryBackingStore);
};

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_ON_DISK_DIRECTORY_BACKING_STORE_H_
