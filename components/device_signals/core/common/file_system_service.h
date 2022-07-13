// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_FILE_SYSTEM_SERVICE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_FILE_SYSTEM_SERVICE_H_

#include <memory>
#include <vector>

namespace base {
class FilePath;
}  // namespace base

namespace device_signals {

enum class PresenceValue;
class PlatformDelegate;
struct FileSystemItem;
struct GetFileSystemInfoOptions;

class FileSystemService {
 public:
  virtual ~FileSystemService() = default;

  // Creates a FileSystemService instance using the given `delegate`.
  static std::unique_ptr<FileSystemService> Create(
      std::unique_ptr<PlatformDelegate> delegate);

  // Collects and returns the file system items' signals as requested by
  // `options`.
  virtual std::vector<FileSystemItem> GetSignals(
      const std::vector<GetFileSystemInfoOptions>& options) = 0;

  // Given an `original_file_path`, will attempt to resolve any environment
  // variables, and store that result in `resolved_file_path`, then calculate
  // the PresenceValue of that file system resource and return that.
  virtual PresenceValue ResolveFileSystemItem(
      const base::FilePath& original_file_path,
      base::FilePath* resolved_file_path) const = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_FILE_SYSTEM_SERVICE_H_
