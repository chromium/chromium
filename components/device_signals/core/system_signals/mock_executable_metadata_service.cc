// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/mock_executable_metadata_service.h"

#include <memory>

#include "components/device_signals/core/system_signals/mock_platform_delegate.h"

namespace device_signals {

MockExecutableMetadataService::MockExecutableMetadataService()
    : ExecutableMetadataService(std::make_unique<MockPlatformDelegate>()) {}

MockExecutableMetadataService::~MockExecutableMetadataService() = default;

}  // namespace device_signals
