// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/swap_management/fake_swap_management_client.h"

namespace {}  // namespace

namespace ash {

FakeSwapManagementClient::FakeSwapManagementClient() = default;

FakeSwapManagementClient::~FakeSwapManagementClient() = default;

void FakeSwapManagementClient::Init(dbus::Bus* bus) {}

void FakeSwapManagementClient::SwapZramEnableWriteback(
    uint32_t size,
    chromeos::VoidDBusMethodCallback callback) {
  std::move(callback).Run(true);
}

void FakeSwapManagementClient::SwapZramSetWritebackLimit(
    uint32_t limit,
    chromeos::VoidDBusMethodCallback callback) {
  std::move(callback).Run(true);
}

void FakeSwapManagementClient::SwapZramMarkIdle(
    uint32_t age,
    chromeos::VoidDBusMethodCallback callback) {
  std::move(callback).Run(true);
}

void FakeSwapManagementClient::InitiateSwapZramWriteback(
    swap_management::ZramWritebackMode mode,
    chromeos::VoidDBusMethodCallback callback) {
  std::move(callback).Run(true);
}

}  // namespace ash
