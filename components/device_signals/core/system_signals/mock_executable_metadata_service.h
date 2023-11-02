// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MOCK_EXECUTABLE_METADATA_SERVICE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MOCK_EXECUTABLE_METADATA_SERVICE_H_

#include "base/files/file_path.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/executable_metadata_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockExecutableMetadataService : public ExecutableMetadataService {
 public:
  MockExecutableMetadataService();
  ~MockExecutableMetadataService() override;

  MOCK_METHOD(FilePathMap<ExecutableMetadata>,
              GetAllExecutableMetadata,
              (const FilePathSet&),
              (override));
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MOCK_EXECUTABLE_METADATA_SERVICE_H_
