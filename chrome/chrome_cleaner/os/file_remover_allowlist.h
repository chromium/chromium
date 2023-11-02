// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_FILE_REMOVER_ALLOWLIST_H_
#define CHROME_CHROME_CLEANER_OS_FILE_REMOVER_ALLOWLIST_H_

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"

namespace chrome_cleaner {

class FileRemoverAllowlist {
 public:
  static FileRemoverAllowlist* GetInstance();

  ~FileRemoverAllowlist();

  // Disables caching, used in unit tests. Tests sometimes override the
  // allowlist and caching them causes problems.
  void DisableCache();

  // Checks if `path` is in the allowlist, indicating it should not be removed.
  bool IsAllowlisted(const base::FilePath& path);

 protected:
  FileRemoverAllowlist();

 private:
  friend struct base::DefaultSingletonTraits<FileRemoverAllowlist>;

  void GenerateFileRemoverAllowlist();

  bool cache_disabled_ = false;
  UnorderedFilePathSet allowlisted_paths_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_FILE_REMOVER_ALLOWLIST_H_
