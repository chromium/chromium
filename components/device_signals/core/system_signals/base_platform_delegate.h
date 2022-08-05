// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_BASE_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_BASE_PLATFORM_DELEGATE_H_

#include "components/device_signals/core/system_signals/platform_delegate.h"

namespace device_signals {

// Implements some functionality that is common to all PlatformDelegate
// specializations.
class BasePlatformDelegate : public PlatformDelegate {
 public:
  ~BasePlatformDelegate() override;

  // PlatformDelegate:
  bool PathIsReadable(const base::FilePath& file_path) const override;
  bool DirectoryExists(const base::FilePath& file_path) const override;
  FilePathMap<ExecutableMetadata> GetAllExecutableMetadata(
      const FilePathSet& file_paths) override;

 protected:
  BasePlatformDelegate();

  // Returns a map of file paths to whether a currently running process was
  // spawned from that file. The set of file paths in the map are specified by
  // `file_paths`.
  FilePathMap<bool> AreExecutablesRunning(const FilePathSet& file_paths);
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_BASE_PLATFORM_DELEGATE_H_
