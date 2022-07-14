// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/linux/linux_system_signals_service.h"

#include <utility>

#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/file_system_service.h"
#include "components/device_signals/core/common/linux/linux_platform_delegate.h"
#include "components/device_signals/core/common/platform_delegate.h"

namespace system_signals {

LinuxSystemSignalsService::LinuxSystemSignalsService()
    : LinuxSystemSignalsService(device_signals::FileSystemService::Create(
          std::make_unique<device_signals::LinuxPlatformDelegate>())) {}

LinuxSystemSignalsService::LinuxSystemSignalsService(
    std::unique_ptr<device_signals::FileSystemService> file_system_service)
    : file_system_service_(std::move(file_system_service)) {}

LinuxSystemSignalsService::~LinuxSystemSignalsService() = default;

void LinuxSystemSignalsService::GetFileSystemSignals(
    const std::vector<device_signals::GetFileSystemInfoOptions>& requests,
    GetFileSystemSignalsCallback callback) {
  std::move(callback).Run(file_system_service_->GetSignals(requests));
}

}  // namespace system_signals
