// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_PRE_FETCHED_PATHS_H_
#define CHROME_CHROME_CLEANER_OS_PRE_FETCHED_PATHS_H_

#include <unordered_map>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"

namespace chrome_cleaner {

// Wrapper for base::PathService with pre-fetched of paths retrieved during
// initialization.
class PreFetchedPaths {
 public:
  static PreFetchedPaths* GetInstance();

  virtual ~PreFetchedPaths();

  // Pre-fetches all paths and returns true if all fetching operations were
  // successful. This method must be explicitly called by the cleaner and the
  // reporter, so an exit code can be emitted if any path cannot be retrieved.
  bool Initialize();

  // Disables prefetching paths. Used in unit tests. Tests sometimes override
  // some of these paths, so prefetching them will cause problems.
  void DisableForTesting();

  // Auxiliary methods for returning pre-fetched paths. These methods should
  // only be called after Initialize(). Crash if for some reason the path to be
  // returned is empty.
  base::FilePath GetExecutablePath() const;
  base::FilePath GetProgramFilesFolder() const;
  base::FilePath GetWindowsFolder() const;
  base::FilePath GetCommonAppDataFolder() const;
  base::FilePath GetLocalAppDataFolder() const;
  base::FilePath GetCsidlProgramFilesFolder() const;
  base::FilePath GetCsidlProgramFilesX86Folder() const;
  base::FilePath GetCsidlWindowsFolder() const;
  base::FilePath GetCsidlStartupFolder() const;
  base::FilePath GetCsidlSystemFolder() const;
  base::FilePath GetCsidlCommonAppDataFolder() const;
  base::FilePath GetCsidlLocalAppDataFolder() const;

 protected:
  PreFetchedPaths();

 private:
  friend struct base::DefaultSingletonTraits<PreFetchedPaths>;

  bool FetchPath(int key);

  // Implements logic for retrieving a value.
  base::FilePath Get(int key) const;

  bool initialized_ = false;
  bool cache_disabled_ = false;
  std::unordered_map<int, base::FilePath> paths_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_PRE_FETCHED_PATHS_H_
