// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MOCK_FILE_SYSTEM_SERVICE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MOCK_FILE_SYSTEM_SERVICE_H_

#include "components/device_signals/core/system_signals/file_system_service.h"

#include "components/device_signals/core/common/common_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockFileSystemService : public FileSystemService {
 public:
  MockFileSystemService();
  ~MockFileSystemService() override;

  MOCK_METHOD(std::vector<FileSystemItem>,
              GetSignals,
              (const std::vector<GetFileSystemInfoOptions>&),
              (override));
  MOCK_METHOD(PresenceValue,
              ResolveFileSystemItem,
              (const base::FilePath&, base::FilePath*),
              (const override));
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MOCK_FILE_SYSTEM_SERVICE_H_
