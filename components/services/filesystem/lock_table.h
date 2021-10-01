// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FILESYSTEM_LOCK_TABLE_H_
#define COMPONENTS_SERVICES_FILESYSTEM_LOCK_TABLE_H_

#include <set>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"

namespace filesystem {

class FileImpl;

// A table of all locks held by this process. We have one global table owned by
// the app, but accessible by everything just in case two connections from the
// same origin try to lock the same file.
class LockTable : public base::RefCounted<LockTable> {
 public:
  LockTable();

  LockTable(const LockTable&) = delete;
  LockTable& operator=(const LockTable&) = delete;

  // Locks a file.
  base::File::Error LockFile(FileImpl* file);

  // Releases a lock, if there is one.
  base::File::Error UnlockFile(FileImpl* file);

  // Removes a path from the list of |locked_files_|. This can be called on
  // files that were never locked, as it is called on all file closes and
  // FileImpl destruction.
  void RemoveFromLockTable(const base::FilePath& path);

 private:
  friend class base::RefCounted<LockTable>;
  ~LockTable();

  // Open, locked files. We keep track of this so we quickly error when we try
  // to double lock a file.
  std::set<base::FilePath> locked_files_;
};

}  // namespace filesystem

#endif  // COMPONENTS_SERVICES_FILESYSTEM_LOCK_TABLE_H_
