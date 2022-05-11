// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/win/win_system_signals_service.h"

namespace system_signals {

WinSystemSignalsService::WinSystemSignalsService(
    mojo::PendingReceiver<device_signals::mojom::SystemSignalsService> receiver)
    : receiver_(this, std::move(receiver)) {}

WinSystemSignalsService::~WinSystemSignalsService() = default;

void WinSystemSignalsService::GetBinarySignals(
    std::vector<device_signals::mojom::BinarySignalsRequestPtr> requests,
    GetBinarySignalsCallback callback) {
  // TODO(b/231298500): Implement this.
  std::move(callback).Run({});
}

void WinSystemSignalsService::GetAntiVirusSignals(
    GetAntiVirusSignalsCallback callback) {
  // TODO(b/230471656): Implement this.
  std::move(callback).Run({});
}

void WinSystemSignalsService::GetHotfixSignals(
    GetHotfixSignalsCallback callback) {
  // TODO(b/230471158): Implement this.
  std::move(callback).Run({});
}

}  // namespace system_signals
