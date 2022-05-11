// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/mac/mac_system_signals_service.h"

namespace system_signals {

MacSystemSignalsService::MacSystemSignalsService() = default;
MacSystemSignalsService::~MacSystemSignalsService() = default;

void LinuxSystemSignalsService::GetBinarySignals(
    std::vector<device_signals::mojom::BinarySignalsRequestPtr> requests,
    GetBinarySignalsCallback callback) {
  // TODO(b/231326198): Implement this.
  std::move(callback).Run({});
}

}  // namespace system_signals
