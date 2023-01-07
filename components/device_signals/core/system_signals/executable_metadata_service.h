// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_EXECUTABLE_METADATA_SERVICE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_EXECUTABLE_METADATA_SERVICE_H_

#include <memory>

#include "components/device_signals/core/system_signals/platform_delegate.h"

namespace device_signals {

struct ExecutableMetadata;

class ExecutableMetadataService {
 public:
  virtual ~ExecutableMetadataService();

  static std::unique_ptr<ExecutableMetadataService> Create(
      std::unique_ptr<PlatformDelegate> platform_delegate);

  // Collects and returns executable metadata for all the files in `file_paths`.
  virtual FilePathMap<ExecutableMetadata> GetAllExecutableMetadata(
      const FilePathSet& file_paths) = 0;

 protected:
  explicit ExecutableMetadataService(
      std::unique_ptr<PlatformDelegate> platform_delegate);

  std::unique_ptr<PlatformDelegate> platform_delegate_;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_EXECUTABLE_METADATA_SERVICE_H_
