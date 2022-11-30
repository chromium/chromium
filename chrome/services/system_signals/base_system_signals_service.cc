// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/linux/linux_system_signals_service.h"

#include <utility>

#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/file_system_service.h"

namespace system_signals {

BaseSystemSignalsService::BaseSystemSignalsService(
    mojo::PendingReceiver<device_signals::mojom::SystemSignalsService> receiver,
    std::unique_ptr<device_signals::FileSystemService> file_system_service)
    : receiver_(this, std::move(receiver)),
      file_system_service_(std::move(file_system_service)) {}

BaseSystemSignalsService::~BaseSystemSignalsService() = default;

void BaseSystemSignalsService::GetFileSystemSignals(
    const std::vector<device_signals::GetFileSystemInfoOptions>& requests,
    GetFileSystemSignalsCallback callback) {
  std::move(callback).Run(file_system_service_->GetSignals(requests));
}

}  // namespace system_signals
