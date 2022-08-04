// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_PLATFORM_DELEGATE_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"

namespace base {
class FilePath;
}  // namespace base

namespace device_signals {

struct ExecutableMetadata;

struct CustomFilePathComparator {
  bool operator()(const base::FilePath& a, const base::FilePath& b) const;
};

template <typename T>
using FilePathMap = base::flat_map<base::FilePath, T, CustomFilePathComparator>;

using FilePathSet = base::flat_set<base::FilePath, CustomFilePathComparator>;

// Interface whose derived types encapsulate OS-specific functionalities.
class PlatformDelegate {
 public:
  virtual ~PlatformDelegate() = default;

  // Wrapper functions around implementation in base/files/file_util.h to allow
  // mocking in tests.
  virtual bool PathIsReadable(const base::FilePath& file_path) const = 0;
  virtual bool DirectoryExists(const base::FilePath& file_path) const = 0;

  // Resolves environment variables and relative markers in `file_path`, and
  // returns the absolute path via `resolved_file_path`. Returns true if
  // successful. For consistency on all platforms, this method will return false
  // if no file system item resides at the end path.
  virtual bool ResolveFilePath(const base::FilePath& file_path,
                               base::FilePath* resolved_file_path) = 0;

  // Collects and return executable metadata for all the files in `file_paths`.
  virtual FilePathMap<ExecutableMetadata> GetAllExecutableMetadata(
      const FilePathSet& file_paths) = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_PLATFORM_DELEGATE_H_
