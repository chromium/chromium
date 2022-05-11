// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/linux/linux_system_signals_service.h"

namespace system_signals {

LinuxSystemSignalsService::LinuxSystemSignalsService() = default;
LinuxSystemSignalsService::~LinuxSystemSignalsService() = default;

void LinuxSystemSignalsService::GetBinarySignals(
    std::vector<device_signals::mojom::BinarySignalsRequestPtr> requests,
    GetBinarySignalsCallback callback) {
  // TODO(b/231326345): Implement this.
  std::move(callback).Run({});
}

}  // namespace system_signals
