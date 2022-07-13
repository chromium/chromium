// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/linux/linux_platform_delegate.h"

#include "base/files/file_path.h"
#include "components/device_signals/core/common/common_types.h"

namespace device_signals {

LinuxPlatformDelegate::LinuxPlatformDelegate() = default;

LinuxPlatformDelegate::~LinuxPlatformDelegate() = default;

ExecutableMetadata LinuxPlatformDelegate::GetExecutableMetadata(
    const base::FilePath& file_path) {
  return ExecutableMetadata();
}

}  // namespace device_signals
