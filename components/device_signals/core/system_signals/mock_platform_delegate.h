// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MOCK_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MOCK_PLATFORM_DELEGATE_H_

#include "base/files/file_path.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/platform_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockPlatformDelegate : public PlatformDelegate {
 public:
  MockPlatformDelegate();
  ~MockPlatformDelegate() override;

  MOCK_METHOD(bool, PathIsReadable, (const base::FilePath&), (const override));
  MOCK_METHOD(bool, DirectoryExists, (const base::FilePath&), (const override));
  MOCK_METHOD(bool,
              ResolveFilePath,
              (const base::FilePath&, base::FilePath*),
              (override));
  MOCK_METHOD(FilePathMap<bool>,
              AreExecutablesRunning,
              (const FilePathSet&),
              (override));
  MOCK_METHOD(std::optional<ProductMetadata>,
              GetProductMetadata,
              (const base::FilePath&),
              (override));
  MOCK_METHOD(std::optional<SigningCertificatesPublicKeys>,
              GetSigningCertificatesPublicKeys,
              (const base::FilePath&),
              (override));
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MOCK_PLATFORM_DELEGATE_H_
