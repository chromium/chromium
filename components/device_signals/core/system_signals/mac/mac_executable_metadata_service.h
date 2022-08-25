// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MAC_MAC_EXECUTABLE_METADATA_SERVICE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MAC_MAC_EXECUTABLE_METADATA_SERVICE_H_

#include <memory>

#include "components/device_signals/core/system_signals/executable_metadata_service.h"

namespace device_signals {

class MacExecutableMetadataService : public ExecutableMetadataService {
 public:
  explicit MacExecutableMetadataService(
      std::unique_ptr<PlatformDelegate> platform_delegate);
  ~MacExecutableMetadataService() override;

  // Collects and return executable metadata for all the files in `file_paths`.
  FilePathMap<ExecutableMetadata> GetAllExecutableMetadata(
      const FilePathSet& file_paths) override;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MAC_MAC_EXECUTABLE_METADATA_SERVICE_H_
